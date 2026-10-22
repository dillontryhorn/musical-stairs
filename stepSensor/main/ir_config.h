#ifndef IR_CONFIG_H
#define IR_CONFIG_H

#include "driver/gpio.h"

/* ============================================================
 * PIN CONFIGURATION
 * ============================================================ */

/*
 * GPIO18 drives both IR LEDs.
 *
 * Wire each LED with its own resistor:
 *
 * GPIO18 ---- 220 ohm ---- IR LED ---- GND
 *        |
 *        +---- 220 ohm ---- IR LED ---- GND
 */
#define IR_TX_GPIO             GPIO_NUM_18

/*
 * IR receiver output.
 *
 * Expected receiver behavior:
 *
 * No 38 kHz IR  -> HIGH
 * 38 kHz IR     -> LOW
 */
#define IR_RECEIVER_GPIO       GPIO_NUM_21

/*
 * Onboard status LED.
 *
 * CLEAR          -> LED OFF
 * BLOCKED        -> LED ON
 * INVALID/FAULT  -> LED ON
 */
#define STATUS_LED_GPIO        GPIO_NUM_2


/* ============================================================
 * RMT / IR CONFIGURATION
 * ============================================================ */

/*
 * 1 MHz RMT resolution:
 *
 * 1 tick = 1 microsecond
 */
#define RMT_RESOLUTION_HZ      1000000


/*
 * Approximate 38 kHz carrier:
 *
 * HIGH = 13 us
 * LOW  = 13 us
 *
 * Period = 26 us
 *
 * 1 / 26 us = 38.46 kHz
 */
#define IR_HIGH_US             13
#define IR_LOW_US              13


/*
 * Number of 38 kHz cycles in one burst.
 *
 * 19 * 26 us = 494 us
 */
#define IR_CARRIER_CYCLES      19


/*
 * Quiet time between bursts.
 */
#define IR_OFF_TIME_US         500


/*
 * How long we'll allow RMT to finish a ~494 us transmission.
 *
 * This is intentionally generous.
 */
#define RMT_TIMEOUT_MS         20


/* ============================================================
 * FILTERING
 * ============================================================ */

/*
 * Require several consecutive measurements before changing
 * the reported state.
 */
#define CLEAR_CONFIRM_COUNT      3
#define BLOCK_CONFIRM_COUNT      3
#define INVALID_CONFIRM_COUNT    3



#endif /* IR_CONFIG_H */
