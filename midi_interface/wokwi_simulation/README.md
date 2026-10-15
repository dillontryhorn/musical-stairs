# Musical stairs in Wokwi

Dillon Tryhorn, 2026

The Wokwi simulation is located at https://wokwi.com/projects/477899126044973057

Eight buttons replace the ESP-NOW footstep messages, and the serial monitor shows the outgoing USB MIDI events. It tests the note logic, without any ESP NOW interfacing or audio output.

## Import into Wokwi to recreate in your browser

1. Open [a new ESP32-S3 project](https://wokwi.com/projects/new/esp32-s3).
2. Above the code editor, open the small down-arrow menu beside Library Manager
   and choose *Upload file(s)*.
3. Select these four files from this folder: `sketch.ino`, `diagram.json`,
   `stair_player.h`, and `stair_message.h`. Replace the template files if prompted. If you cannot replace the files, paste `sketch.ino` and
   `diagram.json` into the existing tabs and add the two headers with *New file*.
4. Click the green Play button. The serial monitor prints the startup instructions.
5. Click the diagram to focus it, then press keys **1-8**, or click the buttons.

Upload the individual files, not this README or a ZIP archive. The folder has everything needed; it does not reference files outside the folder.

## Try it

Each button press starts one note at velocity 100 on MIDI channel 1. By default, the note holds for as long as the button is down and stops when it is released. 

Set `HOLD_NOTES_WHILE_PRESSED` in `stair_player.h` to choose the behavior:

| Flag | Behavior |
| --- | --- |
| `1` (default) | Hold the note until the button/sensor is released. |
| `0` | Play one `NOTE_DURATION_MS` note (300 ms by default) per press. |

In either mode, holding the button does not repeat the note. Release before pressing again. In mode `0`, releasing early lets the note finish its 300 ms, a fresh press before it finishes releases the old note and starts a new one.

| Key/button | Stair index | GPIO | MIDI note |
| --- | --- | --- | --- |
| 1 | 0 | 4 | C4 / 60 |
| 2 | 1 | 5 | D4 / 62 |
| 3 | 2 | 6 | E4 / 64 |
| 4 | 3 | 7 | F4 / 65 |
| 5 | 4 | 8 | G4 / 67 |
| 6 | 5 | 9 | A4 / 69 |
| 7 | 6 | 10 | B4 / 71 |
| 8 | 7 | 11 | C5 / 72 |

- With the default flag `1`, hold 1 longer than 300 ms: expect one Note On for 60 and no Note Off until you release the button (including a Ctrl-click hold).
- Hold 1 and 2 together: both notes stay on. Release 1: only note 60 stops.
- Release and press 1 again: expect Off, then a fresh On.
- With flag `0`, hold 1: expect one Note On and Note Off about 300 ms later, with no repeated notes until you release and press again.

Example output in hold mode, releasing after two seconds (timestamps vary):

```text
[1000 ms] NOTE ON  ch=1 note=60 velocity=100 | USB-MIDI 09 90 3C 64
[3000 ms] NOTE OFF ch=1 note=60 velocity=  0 | USB-MIDI 08 80 3C 00
```

The initial `CONTROL` message with note/controller 120 is All Sound Off. It matches the hardware's initialization when the keyboard connects.

## Relationship to the hardware firmware

The ESP implementation receives two-byte ESP-NOW messages: stair index, then velocity. Velocity `1..127` means pressed, and velocity `0` means released. The simulation reads button states directly instead of sending radio messages.

Change `HOLD_NOTES_WHILE_PRESSED`, the notes, `NOTE_DURATION_MS`, or `MIDI_CHANNEL` in `stair_player.h`. The mode flag must match in the simulation and hardware copies to get the same behavior; rebuild/restart after changing it. Keep one button/pin per note if changing the number of stairs.

The ESP-IDF project in `../midi_esp32s3_implementation` remains the real hardware firmware. Its ESP-NOW receiver and USB host code are not used by this simulation as Wokwi does not support that capability.
