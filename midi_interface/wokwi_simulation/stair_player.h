#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Stair 0 is middle C
static const uint8_t stair_notes[] = {60, 62, 64, 65, 67, 69, 71, 72};
#define STAIR_COUNT (sizeof(stair_notes) / sizeof(stair_notes[0]))

#ifndef HOLD_NOTES_WHILE_PRESSED
#define HOLD_NOTES_WHILE_PRESSED 1 // 1: hold until release; 0: one timed note per press, MODIFY FOR YOUR NEED
#endif

#define NOTE_DURATION_MS 300 // when timed notes
#define MIDI_CHANNEL 0 // MIDI channel 1 on the wire is numbered 0

typedef bool (*midi_sender_t)(uint8_t status, uint8_t note, uint8_t velocity);

typedef struct {
    bool stair_down[STAIR_COUNT];
    // 0 is silent, -1 is held, and positive values are timed note-off deadlines
    int64_t note_until[STAIR_COUNT];
} stair_player_t;

static inline void stair_player_poll(stair_player_t *player, int64_t now, midi_sender_t send)
{
    for (size_t stair = 0; stair < STAIR_COUNT; ++stair) {
        if (player->note_until[stair] > 0 && now >= player->note_until[stair] &&
            send(0x80 | MIDI_CHANNEL, stair_notes[stair], 0)) {
            player->note_until[stair] = 0;
        }
    }
}

// Caller supplies a valid stair and velocity (0: release, 1..127: press)
// Keep an event queued until this returns true, or retry the current button state
static inline bool stair_player_set_input(stair_player_t *player, uint8_t stair,
                                          uint8_t velocity, int64_t now, midi_sender_t send)
{
    if (velocity == 0) {
#if HOLD_NOTES_WHILE_PRESSED
        if (player->note_until[stair] != 0 &&
            !send(0x80 | MIDI_CHANNEL, stair_notes[stair], 0)) {
            return false;
        }
        player->note_until[stair] = 0;
#endif
        player->stair_down[stair] = false;
        return true;
    }
    if (player->stair_down[stair]) {
        // A held signal must not retrigger, even after a one-shot note expires
        return true;
    }
    if (player->note_until[stair] != 0) {
        // Release a previous one-shot note before a fresh press retriggers it
        if (send(0x80 | MIDI_CHANNEL, stair_notes[stair], 0)) {
            player->note_until[stair] = 0;
        }
        return false;
    }
    if (!send(0x90 | MIDI_CHANNEL, stair_notes[stair], velocity)) {
        return false;
    }
    player->stair_down[stair] = true;
#if HOLD_NOTES_WHILE_PRESSED
    (void)now;
    player->note_until[stair] = -1;
#else
    player->note_until[stair] = now + NOTE_DURATION_MS * 1000;
#endif
    return true;
}
