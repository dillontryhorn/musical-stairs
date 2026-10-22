idf_component_register(
    SRCS "main.c" "initialization.c" "transmit.c" "analyze.c" "measurement.c" "step_sender.c"
    INCLUDE_DIRS "." "../../midi_interface/midi_esp32s3/main"
    PRIV_REQUIRES
        esp_driver_gpio
        esp_driver_rmt
        esp_wifi esp_event esp_netif nvs_flash
)
