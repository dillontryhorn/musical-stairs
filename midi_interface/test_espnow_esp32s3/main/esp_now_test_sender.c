// System Headers
#include <assert.h>
#include <stdbool.h>
#include <string.h>

// 3rd Party Headers
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// Project Headers
#include "stair_message.h"

// Dillon Tryhorn 2026

// This is the MAC address of the MIDI receiver ESP32-S3, update it when using another board
// Use FF:FF:FF:FF:FF:FF for broadcast, which has no receiver acknowledgment
static const uint8_t receiver_mac[6] = {0x28, 0x84, 0x85, 0x44, 0x80, 0x88};
#define TEST_VELOCITY 100

static const char *TAG = "test_sender";
static QueueHandle_t send_results;

// Check whether the configured receiver address is the broadcast MAC address
static bool uses_broadcast(void)
{
    const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return memcmp(receiver_mac, broadcast_mac, sizeof(receiver_mac)) == 0;
}

// Queue the ESP NOW send result without blocking the Wi-Fi callback
static void send_done(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    (void)info;
    // Wi-Fi task: report completion without waiting or logging here
    xQueueSend(send_results, &status, 0);
}

// Initialize Wi-Fi and ESP NOW, register the send callback, and add the receiver peer
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
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_done));

    esp_now_peer_info_t peer = 
	{
        .channel = STAIR_WIFI_CHANNEL,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, receiver_mac, sizeof(receiver_mac));
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "Sender MAC: " MACSTR, MAC2STR(mac));
    ESP_LOGI(TAG, "Destination: " MACSTR ", Wi-Fi channel %d", MAC2STR(receiver_mac), STAIR_WIFI_CHANNEL);
    // Explain that a successful broadcast send does not confirm receiver acknowledgment
    if (uses_broadcast()) 
	{
        ESP_LOGI(TAG, "Broadcast TX success has no receiver ACK; check the receiver's RX log.");
    }
    // Explain that unicast acknowledgment confirms radio reception but not USB playback
    else 
	{
        ESP_LOGI(TAG, "Unicast TX success means receiver radio ACK; it does not confirm USB playback.");
    }
}

// Send a stair press or release and log its ESP NOW transmission result
static void send_stair(uint8_t stair, uint8_t velocity)
{
    const stair_message_t message = {stair, velocity};
    ESP_LOGI(TAG, "stair=%u velocity=%u payload=%02X %02X", stair, message.velocity, message.stair, message.velocity);

    esp_err_t err = esp_now_send(receiver_mac, (const uint8_t *)&message, sizeof(message));
    // Log immediate submission errors and skip waiting for a send callback
    if (err != ESP_OK) 
	{
        ESP_LOGE(TAG, "esp_now_send: %s", esp_err_to_name(err));
        return;
    }

    // Keep only one send in flight so its result belongs to this state update.
    esp_now_send_status_t status;
    // Suspend testing if no send callback result arrives within one second
    if (xQueueReceive(send_results, &status, pdMS_TO_TICKS(1000)) != pdTRUE) 
	{
        ESP_LOGE(TAG, "Send callback timed out; reset the sender to retry.");
        vTaskSuspend(NULL); // Do not mistake a late callback for the next send.
        return;
    }
    // Report a successful radio send according to whether the destination is broadcast
    if (status == ESP_NOW_SEND_SUCCESS) 
	{
        // Log broadcast success without claiming receiver acknowledgment
        if (uses_broadcast()) 
		{
            ESP_LOGI(TAG, "ESP-NOW TX: OK (broadcast; no receiver ACK)");
        }
        // Log the receiver's radio acknowledgment without claiming USB playback
        else 
		{
            ESP_LOGI(TAG, "ESP-NOW TX: OK (receiver radio ACK; USB playback unconfirmed)");
        }
    }
    // Report a failed radio transmission
    else 
	{
        ESP_LOGW(TAG, "ESP-NOW TX: FAILED (check receiver power, MAC and channel)");
    }
}

// Initialize the test sender and continuously run the three stair test sequences
void app_main(void)
{
    send_results = xQueueCreate(1, sizeof(esp_now_send_status_t));
    assert(send_results != NULL);
    start_esp_now();
    vTaskDelay(pdMS_TO_TICKS(3000)); // Allow time to open the serial monitor

    // Reset the receiver's press state if this sender was reset during a test
    for (uint8_t stair = 0; stair < 8; ++stair) 
	{
        send_stair(stair, 0);
    }

    // Main execution loop, repeat the ascending, overlapping, and repeated-press tests continuously
    while(1) 
	{
        ESP_LOGI(TAG, "Test 1: ascending stairs (one timed note per press, release after 600 ms)");
        // Press and release each of the eight stairs in ascending order
        for (uint8_t stair = 0; stair < 8; ++stair) 
		{
            send_stair(stair, TEST_VELOCITY);
            vTaskDelay(pdMS_TO_TICKS(600));
            send_stair(stair, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "Test 2: overlapping timed notes on stairs 0, 2, 4, then release separately");
        const uint8_t chord[] = {0, 2, 4};
        // Press stairs 0, 2, and 4 close together to overlap their timed notes
        for (size_t i = 0; i < sizeof(chord); ++i) 
		{
            send_stair(chord[i], TEST_VELOCITY);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        // Release each chord stair separately to rearm it for later presses
        for (size_t i = 0; i < sizeof(chord); ++i) 
		{
            send_stair(chord[i], 0);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        vTaskDelay(pdMS_TO_TICKS(700));

        ESP_LOGI(TAG, "Test 3: repeated positive updates (one note), then release and press again");
        // Repeat the same press update to check that a held stair does not retrigger
        for (int i = 0; i < 3; ++i) 
		{
            send_stair(0, TEST_VELOCITY);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        send_stair(0, 0);
        vTaskDelay(pdMS_TO_TICKS(150));
        send_stair(0, TEST_VELOCITY);
        vTaskDelay(pdMS_TO_TICKS(600));
        send_stair(0, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
