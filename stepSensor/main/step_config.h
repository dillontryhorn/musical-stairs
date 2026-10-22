#pragma once

// Use the station MAC printed by midi_esp32s3, as in the test sender.
#define STEP_RECEIVER_MAC {0x28, 0x84, 0x85, 0x44, 0x80, 0x88}
#define STEP_STAIR_INDEX 0
#define STEP_VELOCITY 100

_Static_assert(STEP_STAIR_INDEX >= 0 && STEP_STAIR_INDEX < 8,
               "The default MIDI receiver supports stairs 0 through 7");
_Static_assert(STEP_VELOCITY >= 1 && STEP_VELOCITY <= 127,
               "Press velocity must be 1 through 127");
