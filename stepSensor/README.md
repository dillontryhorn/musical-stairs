# IR step sensor

ESP-IDF 6.1 firmware ported from StepNote, with ESP-NOW initialization based on
the test sender. Builds for ESP32 (default) or ESP32-S3. The sensor board need not
match the MIDI receiver's ESP32-S3 chip.

## Wiring and configuration

The original wiring is preserved in [main/ir_config.h](main/ir_config.h):

| Signal | GPIO | Behavior |
| --- | --- | --- |
| IR transmitter | 18 | Two IR LEDs, each with its own 220-ohm series resistor |
| IR receiver | 21 | Active low when it detects IR |
| Status LED | 2 | Off when clear; on when blocked, starting, or invalid |

On other boards, choose suitable GPIOs and an external status LED if needed.
The GPIO2 output expects a conventional active-high LED, not an addressable RGB LED.

Set the MIDI receiver's printed station MAC in `STEP_RECEIVER_MAC` in
[main/step_config.h](main/step_config.h). Its default matches the test sender:
`28:84:85:44:80:88`. Assign each sensor its own `STEP_STAIR_INDEX` (0–7), with
`STEP_VELOCITY` between 1 and 127 (default 100).

The build includes the receiver's [stair_message.h](../midi_interface/midi_esp32s3/main/stair_message.h)
directly. Both boards therefore use its two-byte `stair_message_t` and Wi-Fi
channel (default 1). No access point or Wi-Fi credentials are needed.

## Behavior

The RMT transmitter sends 19 cycles at approximately 38.46 kHz after 500 us
of quiet time. A falling-edge ISR records receiver activity during the burst.
Three consecutive measurements confirm clear, blocked, or invalid status.
The measurement loop yields one RTOS tick per measurement; actual response
time includes that tick and RMT scheduling.

A confirmed blockage sends `{stair, 100}` by default. A confirmed clear beam
sends `{stair, 0}`, rearming the receiver. Holding a step does not repeat notes.
Invalid readings keep the diagnostic LED on but do not send a press or rearm
a held step. Startup sends a release to clear the receiver's previous state;
a beam already blocked at startup then triggers once.

A separate task sends transitions in order and repeats the latest state every
250 ms to recover a lost final update or receiver restart. A receiver restart
while blocked can consequently play the held step once. ESP-NOW radio delivery
does not confirm USB MIDI playback. Very short transitions can still be lost
over radio; there is no application-level acknowledgment. A full 32-update
queue is logged. A missing send callback suspends radio sending until reset to
avoid confusing late callbacks; IR sensing and the LED continue running.

## Build and check

From this directory in an ESP-IDF PowerShell prompt:

```powershell
idf.py set-target esp32
# For an ESP32-S3 sensor instead: idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

With the MIDI receiver running, verify:

1. Align the beam: LED turns off and serial status reports CLEAR.
2. Block it: LED turns on and sender logs positive velocity for the configured stair.
3. Keep it blocked: only one timed note plays.
4. Clear and block again: release velocity 0 rearms a new note.
5. Force the receiver output low before a burst: INVALID lights the LED without a new press.
6. Power off the MIDI board: send failures are logged while the sensor LED still responds.

Audio requires the receiver's USB MIDI device to be connected and ready.
