#include "measurement.h"
#include "analyze.h"
#include "transmit.h"
#include "ir_config.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IR_CURTAIN";
static volatile int last_baseline = -1;
static volatile bool last_saw_ir = false;

/* ============================================================
 * PERFORM ONE COMPLETE MEASUREMENT
 * ============================================================ */

static void perform_measurement(void)
{
    /*
     * ========================================================
     * STEP 1:
     *
     * Leave transmitter OFF for 500 us.
     * ========================================================
     */

    esp_rom_delay_us(
        IR_OFF_TIME_US
    );


    /*
     * ========================================================
     * STEP 2:
     *
     * Read baseline while OUR transmitter is definitely off.
     * ========================================================
     */

    int baseline =
        gpio_get_level(
            IR_RECEIVER_GPIO
        );


    last_baseline = baseline;


    /*
     * ========================================================
     * STEP 3:
     *
     * Send exactly 19 cycles of ~38 kHz.
     * ========================================================
     */

    bool tx_success = transmit_ir_burst();


    /*
     * Don't classify a failed RMT transaction.
     */
    if (!tx_success)
    {
        /*
         * Fail safe.
         */
        set_curtain_status(
            CURTAIN_INVALID
        );


        return;
    }


    /*
     * ========================================================
     * STEP 4:
     *
     * Record whether receiver responded during our exact
     * transmit window.
     * ========================================================
     */

    bool detected =
        ir_was_detected();


    last_saw_ir =
        detected;


    /*
     * Extra safety:
     *
     * If the receiver is still LOW immediately after the
     * burst, count that as seeing IR.
     */
    if (gpio_get_level(IR_RECEIVER_GPIO) == 0)
    {
        detected = true;

        last_saw_ir = true;
    }


    /*
     * ========================================================
     * STEP 5:
     *
     * Compare baseline to our transmitted response.
     * ========================================================
     */

    process_measurement(
        baseline,
        detected
    );
}


/* ============================================================
 * MEASUREMENT TASK
 * ============================================================ */

void ir_measurement_task(void *arg)
{
    uint32_t debug_counter = 0;


    while (1)
    {
        /*
         * One cycle takes approximately:
         *
         * 500 us OFF
         *
         * +
         *
         * 494 us transmitting
         *
         * ~994 us total
         *
         * plus RMT scheduling and the one-tick task delay below.
         */
        perform_measurement();

        // Yield to Wi-Fi and idle tasks; a busy polling loop can trip the watchdog.
        vTaskDelay(1);


        /*
         * Don't spam the serial monitor.
         */
        debug_counter++;


        if (debug_counter >= 250)
        {
            debug_counter = 0;


            const char *state;


            switch (get_curtain_status())
            {
                case CURTAIN_CLEAR:

                    state = "CLEAR";

                    break;


                case CURTAIN_BLOCKED:

                    state = "BLOCKED";

                    break;


                case CURTAIN_INVALID:

                    state = "INVALID";

                    break;


                default:

                    state = "STARTING";

                    break;
            }


            ESP_LOGI(
                TAG,
                "baseline=%d  saw_IR=%d  state=%s  timeouts=%lu",
                last_baseline,
                last_saw_ir,
                state,
                (unsigned long)get_rmt_timeout_count()
            );
        }
    }
}


