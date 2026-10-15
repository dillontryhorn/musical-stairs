#pragma once

#include <stdint.h>

// Share this header with the sensor/central-controller firmware (ESP32 or S3)
#define STAIR_WIFI_CHANNEL 1

// Exactly two bytes, in this order. One message triggers one footstep
typedef struct {
    uint8_t stair;    // 0-based index into the receiver's stair_notes[]
    uint8_t velocity; // 1..127; use 100 for a fixed volume
} stair_message_t;

//Note velocity is a MIDI measurement that tracks how hard or soft a player strikes a key or pad