#pragma once

// System Headers
#include <stdint.h>

// Dillon Tryhorn 2026

// Change to use different channel, keep same between transmitter and receiver
#define STAIR_WIFI_CHANNEL 1

typedef struct 
{
    uint8_t stair;    // stair index, 0 to n stairs
    uint8_t velocity; // 1-127 = pressed (project defaults to 100), 0 = released
} stair_message_t;
