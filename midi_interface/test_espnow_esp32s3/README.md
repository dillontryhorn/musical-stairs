# ESP-NOW Test Sender for ESP32-S3 Device

Dillon Tryhorn 2026

ESP32-S3 Test Sender -> ESP-NOW -> MIDI Receiver -> Keyboard

This sends stair press/release messages from a second ESP32-S3 to the
[MIDI receiver](../midi_esp32s3/README.md). It uses ESP-IDF 6.1 and runs the tests automatically.

## Setup

1. Flash the MIDI receiver through its **USB-UART** port and leave its serial monitor open.
2. Set `receiver_mac` in `main/esp_now_test_sender.c` to the printed `MIDI receiver MAC`.
3. Keep both boards powered and use separate terminals for their monitors. For audio, connect
   and power the keyboard and wait for `Keyboard ready` in the receiver monitor.

## Build

From an ESP-IDF PowerShell prompt:

```powershell
cd \path\to\musical-stairs\midi_interface\test_espnow_esp32s3
idf.py set-target esp32s3
idf.py build
idf.py -p <COM_PORT> flash monitor

# If you need help finding the correct COM port
[System.IO.Ports.SerialPort]::GetPortNames()
python -m esptool -p <COM_PORT> chip-id
```

Use the sender's **USB-UART** port and its own COM port.

## Tests

The sender automatically repeats these tests:

- Ascending stairs 0-7, releasing each after a 600 ms delay.
- Overlapping notes on stairs 0, 2, and 4, then separate releases.
- Repeated press updates on stair 0, then a release and new press to check retriggering.

Each press plays once for the receiver's `NOTE_DURATION_MS` (500 ms by default).
Releases rearm the next press without shortening a playing note. Resetting the sender
first sends releases for all eight stairs.

## ESP-NOW interface

The 2-byte message is defined in `main/stair_message.h`. Both boards use Wi-Fi channel
**1** by default; keep `STAIR_WIFI_CHANNEL` the same in both projects.

| Byte | Meaning |
| --- | --- |
| 0 | Stair index: `0-7` |
| 1 | `1-127` = press velocity (use `100`); `0` = release |

Set all six `receiver_mac` bytes to `0xFF` for broadcast, which has no receiver acknowledgment.

## Troubleshooting and Debug

The sender logs `ESP-NOW TX: OK` or `ESP-NOW TX: FAILED`. Unicast `OK` confirms the
receiver's radio acknowledgment. Check the receiver's `ESP-NOW RX from ...` log for
reception; audible playback also needs a ready keyboard. Radio reception can be checked
without a keyboard.

If sending fails, check receiver power, `receiver_mac`, and matching Wi-Fi channels.
Send errors are logged without retries. If `Send callback timed out` appears, reset the
sender; the test stops after one second without a callback result.
