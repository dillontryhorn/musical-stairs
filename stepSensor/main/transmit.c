#include "transmit.h"
#include "ir_config.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "IR_CURTAIN";
static rmt_channel_handle_t ir_tx_channel = NULL;
static rmt_encoder_handle_t ir_encoder = NULL;
/* Shared between the measurement task and GPIO ISR. */
static volatile bool tx_window_active = false;
static volatile bool saw_ir_during_burst = false;
static volatile uint32_t rmt_timeout_count = 0;

/* ============================================================
 * RMT BURST DATA
 * ============================================================ */

/*
 * One RMT symbol represents one complete carrier cycle:
 *
 * HIGH for 13 us
 * LOW  for 13 us
 *
 *
 *       13 us       13 us
 *
 *     ┌───────┐
 *     │       │
 * ────┘       └────────
 *
 * <------ 26 us ------>
 *
 * approximately 38.46 kHz
 */


/*
 * Array containing the complete burst.
 *
 * 19 symbols =
 *
 * 19 carrier cycles
 *
 * approximately 494 us total.
 */
static rmt_symbol_word_t ir_burst[IR_CARRIER_CYCLES];


/* ============================================================
 * BUILD RMT BURST
 * ============================================================ */

void build_ir_burst(void)
{
    for (int i = 0; i < IR_CARRIER_CYCLES; i++)
    {
        ir_burst[i].level0 = 1;

        ir_burst[i].duration0 =
            IR_HIGH_US;


        ir_burst[i].level1 = 0;

        ir_burst[i].duration1 =
            IR_LOW_US;
    }
}


/* ============================================================
 * RECEIVER GPIO ISR
 * ============================================================ */

/*
 * Receiver output is active LOW.
 *
 * We configure GPIO21 for falling-edge interrupts.
 *
 * If a falling edge occurs while our known IR burst is
 * being transmitted, we record that our IR was detected.
 */
void IRAM_ATTR receiver_gpio_isr(void *arg)
{
    if (!tx_window_active)
    {
        return;
    }


    /*
     * Make sure receiver really is LOW.
     */
    if (gpio_get_level(IR_RECEIVER_GPIO) == 0)
    {
        saw_ir_during_burst = true;
    }
}


/* ============================================================
 * RMT CONFIGURATION
 * ============================================================ */

void configure_rmt(void)
{
    /* --------------------------------------------------------
     * TX CHANNEL
     * ------------------------------------------------------ */

    rmt_tx_channel_config_t tx_config = {

        .clk_src =
            RMT_CLK_SRC_DEFAULT,

        .gpio_num =
            IR_TX_GPIO,

        /*
         * Our payload is only 19 symbols.
         *
         * 64 symbols gives plenty of space.
         */
        .mem_block_symbols =
            64,

        .resolution_hz =
            RMT_RESOLUTION_HZ,

        .trans_queue_depth =
            1
    };


    ESP_ERROR_CHECK(
        rmt_new_tx_channel(
            &tx_config,
            &ir_tx_channel
        )
    );


    /* --------------------------------------------------------
     * COPY ENCODER
     * ------------------------------------------------------ */

    /*
     * The copy encoder simply copies our predefined
     * rmt_symbol_word_t array into RMT memory.
     *
     * This config currently has no fields, hence {}.
     */
    rmt_copy_encoder_config_t encoder_config = {};


    ESP_ERROR_CHECK(
        rmt_new_copy_encoder(
            &encoder_config,
            &ir_encoder
        )
    );


    /* --------------------------------------------------------
     * ENABLE CHANNEL
     * ------------------------------------------------------ */

    ESP_ERROR_CHECK(
        rmt_enable(
            ir_tx_channel
        )
    );
}


/* ============================================================
 * TRANSMIT ONE BURST
 * ============================================================ */

bool transmit_ir_burst(void)
{
    /*
     * One transmission.
     *
     * No looping.
     */
    rmt_transmit_config_t transmit_config = {};


    transmit_config.loop_count = 0;


    /*
     * Open receiver detection window BEFORE starting RMT.
     */
    saw_ir_during_burst = false;

    tx_window_active = true;


    /*
     * Queue the burst.
     */
    esp_err_t result =
        rmt_transmit(
            ir_tx_channel,
            ir_encoder,
            ir_burst,
            sizeof(ir_burst),
            &transmit_config
        );


    if (result != ESP_OK)
    {
        tx_window_active = false;


        ESP_LOGE(
            TAG,
            "rmt_transmit failed: %s",
            esp_err_to_name(result)
        );


        return false;
    }


    /*
     * Wait for THIS finite transmission to complete.
     *
     * The waveform itself only lasts ~494 us.
     */
    result =
        rmt_tx_wait_all_done(
            ir_tx_channel,
            RMT_TIMEOUT_MS
        );


    /*
     * Close receiver detection window.
     */
    tx_window_active = false;


    /*
     * Do NOT ESP_ERROR_CHECK() here.
     *
     * A runtime timeout should not intentionally reboot
     * the ESP32.
     */
    if (result != ESP_OK)
    {
        rmt_timeout_count++;


        ESP_LOGE(
            TAG,
            "RMT timeout/error: %s  count=%lu",
            esp_err_to_name(result),
            (unsigned long)rmt_timeout_count
        );


        return false;
    }


    return true;
}


bool ir_was_detected(void)
{
    return saw_ir_during_burst;
}

uint32_t get_rmt_timeout_count(void)
{
    return rmt_timeout_count;
}
