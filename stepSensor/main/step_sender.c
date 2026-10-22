#include "step_sender.h"
#include "step_config.h"
#include "stair_message.h"

#include <string.h>
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

static const char *TAG = "step_sender";
static const uint8_t receiver_mac[6] = STEP_RECEIVER_MAC;
static QueueHandle_t updates;
static QueueHandle_t send_results;

_Static_assert(sizeof(stair_message_t) == 2, "Expected two-byte stair protocol");

static void send_done(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    (void)info;
    xQueueSend(send_results, &status, 0);
}

// Radio waits run independently so a missing receiver cannot stop IR or LED updates.
static void sender_task(void *arg)
{
    (void)arg;
    stair_message_t message = {STEP_STAIR_INDEX, 0};
    while (true) {
        esp_err_t err = esp_now_send(receiver_mac, (const uint8_t *)&message,
                                     sizeof(message));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_send: %s", esp_err_to_name(err));
        } else {
            esp_now_send_status_t status;
            if (xQueueReceive(send_results, &status, pdMS_TO_TICKS(1000)) != pdTRUE) {
                // A late callback must never be attributed to another packet.
                ESP_LOGE(TAG, "Send callback timed out; reset sensor to restart radio sends. IR/LED remain active.");
                while (true) {
                    vTaskSuspend(NULL);
                }
            }
            if (status != ESP_NOW_SEND_SUCCESS) {
                ESP_LOGW(TAG, "TX failed: stair=%u velocity=%u; check receiver MAC, power and channel",
                         message.stair, message.velocity);
            }
        }
        // Repeat current state to recover a lost update or receiver restart.
        // Receiver deduplicates positive updates until it receives a release.
        if (xQueueReceive(updates, &message, pdMS_TO_TICKS(250)) == pdTRUE) {
            ESP_LOGI(TAG, "stair=%u velocity=%u", message.stair, message.velocity);
        }
    }
}

void step_sender_set_pressed(bool pressed)
{
    const stair_message_t message = {
        .stair = STEP_STAIR_INDEX,
        .velocity = pressed ? STEP_VELOCITY : 0,
    };
    if (xQueueSend(updates, &message, 0) != pdTRUE) {
        ESP_LOGE(TAG, "State queue full; stair update lost");
    }
}

void step_sender_init(void)
{
    updates = xQueueCreate(32, sizeof(stair_message_t));
    send_results = xQueueCreate(1, sizeof(esp_now_send_status_t));
    ESP_ERROR_CHECK(updates && send_results ? ESP_OK : ESP_ERR_NO_MEM);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
    esp_now_peer_info_t peer = {
        .channel = STAIR_WIFI_CHANNEL,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, receiver_mac, sizeof(receiver_mac));
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    ESP_LOGI(TAG, "Destination: " MACSTR ", channel %d, stair %d",
             MAC2STR(receiver_mac), STAIR_WIFI_CHANNEL, STEP_STAIR_INDEX);
    // The task sends a startup release before consuming sensor transitions.
    ESP_ERROR_CHECK(xTaskCreate(sender_task, "step_sender", 4096, NULL, 5, NULL)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
