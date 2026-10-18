#pragma once

// System Headers
#include <stdbool.h>
#include <stdint.h>

// Dillon Tryhorn 2026

void usb_midi_init(void);
void usb_midi_poll(void);
bool usb_midi_ready(void);

// Submit one three-byte MIDI channel message (including Note On/Off)
// True means queued, false means busy, disconnected, or invalid
bool usb_midi_send(uint8_t status, uint8_t note, uint8_t velocity);
