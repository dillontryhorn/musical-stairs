# Musical Stairs
More to come

## MIDI Interface

COMING SOON
See [midi_interface/midi_esp32s3_implementation/README.md](midi_interface/midi_esp32s3_implementation/README.md) for the ESP32-S3 USB MIDI host, ESP-NOW message format, and keyboard wiring.

See [midi_interface/wokwi_simulation/README.md](midi_interface/wokwi_simulation/README.md) for the standalone Wokwi button/serial simulation and browser import steps.

COMING SOON
See [midi_interface/midi_esp32s3_test_sender/README.md](midi_interface/midi_esp32s3_test_sender/README.md) for a second ESP32-S3 that sends test presses and releases to the MIDI receiver.

Both the firmware and Wokwi copies of `stair_player.h` use `HOLD_NOTES_WHILE_PRESSED`: `1` (default) sustains until release; `0` plays one 300 ms note per press. Keep the flag the same in both copies. ESP-NOW senders must use the two-byte stair/velocity format and send both press and release states.

## Minimal ESP Instructions

Open an ESP-IDF enabled powershell prompt

cd <to/target/directory>

idf.py set-target esp32 # for ESP32
idf.py set-target esp32s3 # for ESP32-S3

idf.py build

idf.py flash monitor # auto detects active ESP module, flashes the board, and opens a serial monitor
idf.py -p COM5 flash monitor # manually overrides ESP COM location

Ctrl+] # exit the serial monitor
