#pragma once

// System Headers
#include <stdint.h>

// Dillon Tryhorn 2026

#define STAIR_WIFI_CHANNEL 1

typedef struct 
{
    uint8_t stair;    // 0-based index into the receiver's stair_notes[]
    uint8_t velocity; // 1..127 = pressed, 0 = released
} stair_message_t;
