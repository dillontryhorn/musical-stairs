#pragma once

#include <stdint.h>

#define STAIR_WIFI_CHANNEL 1

// Exactly two bytes, in this order. Report both press and release states.
typedef struct {
    uint8_t stair;    // 0-based index into the receiver's stair_notes[]
    uint8_t velocity; // 1..127 = pressed, 0 = released
} stair_message_t;

//Note velocity is a MIDI measurement that tracks how hard or soft a player strikes a key or pad
