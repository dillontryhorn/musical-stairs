#include "analyze.h"
#include "ir_config.h"
#include "step_sender.h"
#include <stdint.h>
#include "esp_log.h"

static const char *TAG = "IR_CURTAIN";
static volatile curtain_status_t curtain_status = CURTAIN_STARTING;
static uint32_t clear_count = 0;
static uint32_t blocked_count = 0;
static uint32_t invalid_count = 0;
static bool step_pressed = false;

/* ============================================================
 * STATUS HANDLING
 * ============================================================ */

void set_curtain_status(
    curtain_status_t new_status)
{
    /*
     * Don't repeat logs / GPIO writes unless the state
     * actually changed.
     */
    if (new_status == curtain_status)
    {
        return;
    }


    curtain_status = new_status;

    // Faults do not trigger notes or rearm a held step.
    if (new_status == CURTAIN_BLOCKED && !step_pressed) {
        step_sender_set_pressed(true);
        step_pressed = true;
    } else if (new_status == CURTAIN_CLEAR && step_pressed) {
        step_sender_set_pressed(false);
        step_pressed = false;
    }


    switch (new_status)
    {
        /* ----------------------------------------------------
         * CLEAR
         * -------------------------------------------------- */

        case CURTAIN_CLEAR:

            gpio_set_level(
                STATUS_LED_GPIO,
                0
            );

            ESP_LOGI(
                TAG,
                "CURTAIN CLEAR"
            );

            break;


        /* ----------------------------------------------------
         * BLOCKED
         * -------------------------------------------------- */

        case CURTAIN_BLOCKED:

            gpio_set_level(
                STATUS_LED_GPIO,
                1
            );

            ESP_LOGW(
                TAG,
                "CURTAIN BLOCKED"
            );

            break;


        /* ----------------------------------------------------
         * INVALID / FAULT
         * -------------------------------------------------- */

        case CURTAIN_INVALID:

            gpio_set_level(
                STATUS_LED_GPIO,
                1
            );

            ESP_LOGE(
                TAG,
                "DETECTOR INVALID"
            );

            break;


        /* ----------------------------------------------------
         * STARTING
         * -------------------------------------------------- */

        case CURTAIN_STARTING:

        default:

            gpio_set_level(
                STATUS_LED_GPIO,
                1
            );

            break;
    }
}


/* ============================================================
 * PROCESS A COMPLETE MEASUREMENT
 * ============================================================ */

void process_measurement(
    int baseline,
    bool saw_ir)
{
    /*
     * ========================================================
     * CLEAR
     * ========================================================
     *
     * Before transmitting:
     *
     * receiver HIGH
     *
     * During our burst:
     *
     * receiver goes LOW
     *
     *
     * HIGH -> LOW
     *
     * This is our desired response.
     */

    if ((baseline == 1) && saw_ir)
    {
        blocked_count = 0;

        invalid_count = 0;


        if (clear_count < CLEAR_CONFIRM_COUNT)
        {
            clear_count++;
        }


        if (clear_count >= CLEAR_CONFIRM_COUNT)
        {
            set_curtain_status(
                CURTAIN_CLEAR
            );
        }
    }


    /*
     * ========================================================
     * BLOCKED
     * ========================================================
     *
     * Receiver starts HIGH.
     *
     * We transmit our burst.
     *
     * Receiver never goes LOW.
     *
     * Therefore the receiver didn't see our IR.
     */

    else if ((baseline == 1) && !saw_ir)
    {
        clear_count = 0;

        invalid_count = 0;


        if (blocked_count < BLOCK_CONFIRM_COUNT)
        {
            blocked_count++;
        }


        if (blocked_count >= BLOCK_CONFIRM_COUNT)
        {
            set_curtain_status(
                CURTAIN_BLOCKED
            );
        }
    }


    /*
     * ========================================================
     * INVALID
     * ========================================================
     *
     * Receiver was already LOW before we transmitted.
     *
     * We therefore don't have a valid baseline.
     */

    else
    {
        clear_count = 0;

        blocked_count = 0;


        if (invalid_count < INVALID_CONFIRM_COUNT)
        {
            invalid_count++;
        }


        if (invalid_count >= INVALID_CONFIRM_COUNT)
        {
            set_curtain_status(
                CURTAIN_INVALID
            );
        }
    }
}


curtain_status_t get_curtain_status(void)
{
    return curtain_status;
}
