idf_component_register(SRCS "midi_interface.c" "usb_midi.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash usb)
