# MIDI Interface for ESP32-S3 Device

Dillon Tryhorn 2026

ESP-NOW Stair Press/Release -> ESP32-S3 USB Host -> MIDI Device Input

This implements ascending notes, overlapping timed notes, repeated footsteps,
and MIDI output. It uses ESP-IDF 6.1 and Espressif's USB host library

## Keyboard and Wiring

The CASIO Casiotone CT-S190 has a USB Micro-B device port. The S3 acts as its USB host,
replacing the computer in [Casio's CT-S190 connection instructions, EN-30](https://www.casio.com/content/dam/casio/global/support/manuals/electronic-musical-instruments/pdf/008-en/c/CTS190_usersguide_EN.pdf).

1. Flash and monitor through the board's **USB-UART** port.
2. Connect the keyboard to the ESP32-S3's **native USB/OTG** port with a data cable and host adapter
3. Power on the keyboard. Do not run any built-in song before testing MIDI.

See [Espressif's USB host documentation](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html) for more info on the USB Host Library.

## Build

From an ESP-IDF PowerShell prompt:

```powershell
cd \path\to\musical-stairs\midi_interface\midi_esp32s3
idf.py set-target esp32s3
idf.py build
idf.py -p <COM_PORT> flash monitor

# If you need help finding the correct COM port
[System.IO.Ports.SerialPort]::GetPortNames()
python -m esptool -p <COM_PORT> chip-id
```

The log prints the receiver's station MAC and Wi-Fi channel. On USB connection,
`Keyboard Ready` confirms discovery of a MIDI device. The S3 sends
`All Sound Off` on channel 1 when connecting, then waits for stair events.

## ESP-NOW interface

For a second-board test, use
[`../test_espnow_esp32s3`](../test_espnow_esp32s3/README.md). It sends press/release pairs for ascending notes, a chord, and repeated footsteps automatically.

The 2-byte message is defined in `main/stair_message.h` and is shared between the sender and receiver

| Byte | Meaning |
| --- | --- |
| 0 | Stair index: `0..7` by default |
| 1 | `1..127` = press velocity (use `100`); `0` = release |


## Troubleshooting and Debug

The receiver logs `ESP-NOW RX from ... stair=... velocity=... RSSI=...` for each
valid stair message, even without a keyboard. `RSSI` is the received signal
strength in dBm. A length or value mismatch logs a rejection instead.

`Waiting for USB MIDI keyboard` means the USB MIDI output is unavailable. Incoming stair messages are still logged with
`Discarded: USB MIDI keyboard not ready` when MIDI device is unavailable. Check the USB/OTG data connection and keyboard power
if having these issues. The USB-UART socket used for flashing and monitoring is separate from the native USB host connection to the keyboard.

