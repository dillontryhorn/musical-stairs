#pragma once

// System Headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Dillon Tryhorn 2026

// Defining 8 stairs here each with their own notes, stair 0 is middle C, modify this line for more or less stairs
static const uint8_t stair_notes[] = {60, 62, 64, 65, 67, 69, 71, 72};

#define STAIR_COUNT (sizeof(stair_notes) / sizeof(stair_notes[0]))

#define NOTE_DURATION_MS 500 // One timed note per press

#define MIDI_CHANNEL 0 // MIDI channel 1 is numbered 0, 0-15 available

typedef bool (*midi_sender_t)(uint8_t status, uint8_t note, uint8_t velocity);

typedef struct 
{
    bool stair_down[STAIR_COUNT];
    // 0 is silent, positive values are timed notes
    int64_t note_until[STAIR_COUNT];
} stair_player_t;

// Stop expired stair notes once their MIDI note-off messages are accepted
static inline void stair_player_poll(stair_player_t *player, int64_t now, midi_sender_t send)
{
    // Check every stair for an expired timed note
    for (size_t stair = 0; stair < STAIR_COUNT; stair++) 
	{
        // Clear an expired note only after its MIDI note-off message is accepted
        if (player->note_until[stair] > 0 && now >= player->note_until[stair] && send(0x80 | MIDI_CHANNEL, stair_notes[stair], 0)) 
		{
            player->note_until[stair] = 0;
        }
    }
}

// Handle stair releases and new presses while retrying MIDI messages when needed
// Caller supplies a valid stair and velocity (0: release, 1-127: press)
static inline bool stair_player_set_input(stair_player_t *player, uint8_t stair,
                                          uint8_t velocity, int64_t now, midi_sender_t send)
{
    // Mark a released stair as up without stopping its timed note
    if (velocity == 0) 
	{
        player->stair_down[stair] = false;
        return true;
    }
    // Ignore repeated presses until the stair is released
    if (player->stair_down[stair]) 
	{
        // Repeated pressed states must not retrigger
        return true;
    }
    // Finish the previous timed note before retrying a new press
    if (player->note_until[stair] != 0) 
	{
        // Stop the previous timed note before a new press retriggers it
        if (send(0x80 | MIDI_CHANNEL, stair_notes[stair], 0)) 
		{
            player->note_until[stair] = 0;
        }
        return false;
    }
    // Retry the press later if its MIDI note-on message cannot be sent
    if (!send(0x90 | MIDI_CHANNEL, stair_notes[stair], velocity)) 
	{
        return false;
    }
    player->stair_down[stair] = true; // Trigger note
    player->note_until[stair] = now + NOTE_DURATION_MS * 1000; // Current time with defined note duration
    return true;
}
