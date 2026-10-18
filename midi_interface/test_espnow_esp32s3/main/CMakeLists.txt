idf_component_register(SRCS "esp_now_test_sender.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash)
