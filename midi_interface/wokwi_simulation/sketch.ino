#include "esp_timer.h"
#include "stair_player.h"

// Buttons stand in for footstep messages from the other ESP32 boards
static const uint8_t button_pins[] = {4, 5, 6, 7, 8, 9, 10, 11};
static_assert(sizeof(button_pins) == STAIR_COUNT, "Provide one button per stair"); //when adding more stairs
static stair_player_t player;

// Serial output stands in for the keyboard's USB MIDI connection
static bool send_midi(uint8_t status, uint8_t note, uint8_t velocity)
{
    const char *event = (status & 0xF0) == 0x90 ? "NOTE ON " :
                        (status & 0xF0) == 0x80 ? "NOTE OFF" : "CONTROL ";
    Serial.printf("[%lld ms] %s ch=%u note=%u velocity=%3u | USB-MIDI %02X %02X %02X %02X\n",
                  (long long)(esp_timer_get_time() / 1000), event,
                  (status & 0x0F) + 1, note, velocity, status >> 4, status, note, velocity);
    return true;
}

void setup()
{
    Serial.begin(115200);
    for (size_t stair = 0; stair < STAIR_COUNT; stair++) {
        pinMode(button_pins[stair], INPUT_PULLUP);
    }
    Serial.println("Musical stairs: Wokwi simulation");
#if HOLD_NOTES_WHILE_PRESSED
    Serial.println("Notes hold while a button is down and stop on release.");
#else
    Serial.printf("Each button press triggers one %d ms note; holding does not repeat.\n", NOTE_DURATION_MS);
#endif
    Serial.println("This simulator cannot generate audio or use ESP-NOW, so it is mainly used for validating USB MIDI messages.");
    send_midi(0xB0 | MIDI_CHANNEL, 120, 0); // All sound off
}

void loop()
{
    int64_t now = esp_timer_get_time();
    stair_player_poll(&player, now, send_midi);

    for (size_t stair = 0; stair < STAIR_COUNT; stair++) {
        bool pressed = digitalRead(button_pins[stair]) == LOW;
        stair_player_set_input(&player, (uint8_t)stair, pressed ? 100 : 0, now, send_midi);
    }
    delay(1);
}
