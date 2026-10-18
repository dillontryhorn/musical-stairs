// System Headers
#include <assert.h>
#include <string.h>

// 3rd Party Headers
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// Project Headers
#include "stair_message.h"
#include "stair_player.h"
#include "usb_midi.h"

// Dillon Tryhorn 2026

// Increase or decrease if having queue issues
#define STAIR_EVENT_QUEUE_LENGTH 32 

static const char *TAG = "stairs"; // For this specific project
static QueueHandle_t stair_events; // Queue for message passing

// Project-defined event struct
typedef struct 
{
    stair_message_t message;
    uint8_t source[6]; // Source MAC address
    int length;
    int rssi; // Received signal strength indicator
} stair_event_t;

// Receiving a stair trigger from ESP NOW interface
static void receive_stair(const esp_now_recv_info_t *info, const uint8_t *data, int length)
{
	// Ignore packets without receive metadata, a source address, or payload data
    if (info == NULL || info->src_addr == NULL || data == NULL) 
	{
        return;
    }
	
	// Build the event struct
    stair_event_t event = 
	{
        .length = length,
        // Use the received signal strength when available, otherwise default to zero
        .rssi = info->rx_ctrl != NULL ? info->rx_ctrl->rssi : 0,
    };
    memcpy(event.source, info->src_addr, sizeof(event.source));
    // Copy the stair message only when the packet has the expected size
    if (length == sizeof(event.message)) 
	{
        memcpy(&event.message, data, sizeof(event.message));
    }
	
	// Send the event, log if queue full
    if (xQueueSend(stair_events, &event, 0) != pdTRUE) 
	{
        ESP_LOGW(TAG, "Stair event queue full; dropped packet from " MACSTR,
                 MAC2STR(event.source));
    }
}

// Check if event is formed correctly
static bool valid_stair_event(const stair_event_t *event)
{
    return event->length == sizeof(event->message) &&
           event->message.stair < STAIR_COUNT && event->message.velocity <= 127;
}

// Log event to serial console
static void log_stair_event(const stair_event_t *event, bool keyboard_ready)
{
    // Report packets whose length does not match the stair message format
    if (event->length != sizeof(event->message)) 
	{
        ESP_LOGW(TAG, "ESP-NOW RX from " MACSTR ": rejected %d-byte packet (expected %u)",
                 MAC2STR(event->source), event->length, (unsigned)sizeof(event->message));
    }
    // Report messages containing an invalid stair number or velocity
    else if (!valid_stair_event(event)) 
	{
        ESP_LOGW(TAG, "ESP-NOW RX from " MACSTR ": rejected stair=%u velocity=%u",
                 MAC2STR(event->source), event->message.stair, event->message.velocity);
    }
    // Log valid stair events with the sender, velocity, and signal strength
    else 
	{
        ESP_LOGI(TAG, "ESP-NOW RX from " MACSTR " stair=%u velocity=%u RSSI=%d%s",
                 MAC2STR(event->source), event->message.stair, event->message.velocity,
                 // Add a discard notice when the USB MIDI keyboard is unavailable
                 event->rssi, keyboard_ready ? "" : " (discarded: USB MIDI keyboard not ready)");
    }
}

// Initialize ESP NOW interface, log output to serial console for debug
static void start_esp_now(void)
{
    esp_err_t err = nvs_flash_init();
    // Erase and retry NVS initialization if storage is full or uses an incompatible version
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
	{
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_channel(STAIR_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_stair));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "MIDI receiver MAC: " MACSTR ", Wi-Fi channel %d", MAC2STR(mac), STAIR_WIFI_CHANNEL);
}

// Initialize USB MIDI and ESP NOW, then process stair events and timed notes
void app_main(void)
{
	// Create the receive queue, initialize USB MIDI interface, and start ESP NOW interface
    stair_events = xQueueCreate(STAIR_EVENT_QUEUE_LENGTH, sizeof(stair_event_t));
    assert(stair_events != NULL);
    usb_midi_init();
    start_esp_now();
    ESP_LOGI(TAG, "One %d ms note per stair press", NOTE_DURATION_MS);

    stair_player_t player = {0};
    bool was_ready = false;
    bool waiting_for_keyboard = false;
    bool head_logged = false;

	// Main execution loop, continuously handle keyboard connections, stair events, and timed note releases
    while(1) 
	{
		// Check for device 
        usb_midi_poll();
        // Discard incoming stair events and reset playback while the keyboard is unavailable
        if (!usb_midi_ready()) 
		{
            // Log the wait message once until a keyboard becomes ready
            if (!waiting_for_keyboard) 
			{
                ESP_LOGW(TAG, "Waiting for USB MIDI keyboard; ESP-NOW RX will still be logged, "
                              "but stair events will be ignored until MIDI device ready");
                waiting_for_keyboard = true;
            }
            // Do not replay stair events that happened while the keyboard was absent
            // Drain the queue with log events instead of resetting the receive queue
            stair_event_t event;
            // Log and discard up to one queue of pending stair events
            for (int i = 0; i < STAIR_EVENT_QUEUE_LENGTH && xQueueReceive(stair_events, &event, 0) == pdTRUE; i++) 
			{
                log_stair_event(&event, false);
            }
            memset(&player, 0, sizeof(player));
            was_ready = false;
            head_logged = false;
        }
        // Send All Sound Off after connection and retry until the message is queued
        else if (!was_ready) 
		{ // Also stop any note left behind if the ESP32-S3 restarted mid-note
            waiting_for_keyboard = false;
            was_ready = usb_midi_send(0xB0 | MIDI_CHANNEL, 120, 0); // All sound off
        }
        // Release expired notes and process queued stair events once the keyboard is ready
        else 
		{
			// Once everything is initalized and MIDI device is connected 
            int64_t now = esp_timer_get_time();
            stair_player_poll(&player, now, usb_midi_send);

            stair_event_t event;
            // Inspect the oldest stair event without removing it from the queue
            if (xQueuePeek(stair_events, &event, 0) == pdTRUE) 
			{
                // Log the oldest event only once while it waits to be processed
                if (!head_logged) 
				{
                    log_stair_event(&event, true);
                    head_logged = true;
                }
                // Remove invalid events or events whose input update was accepted by the player
                if (!valid_stair_event(&event) ||
                    stair_player_set_input(&player, event.message.stair, event.message.velocity, now, usb_midi_send)) 
			    {
                    xQueueReceive(stair_events, &event, 0);
                    head_logged = false;
                }
            }
        }
		// Update very frequently to reduce latency, change this if having issues
        vTaskDelay(1);
    }
}
