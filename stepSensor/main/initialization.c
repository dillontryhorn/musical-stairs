#include "initialization.h"
#include "transmit.h"
#include "ir_config.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"

static const char *TAG = "IR_CURTAIN";

/* ============================================================
 * GPIO CONFIGURATION
 * ============================================================ */

static void configure_gpio(void)
{
    /* --------------------------------------------------------
     * IR RECEIVER
     * ------------------------------------------------------ */

    gpio_config_t receiver_config = {

        .pin_bit_mask =
            (1ULL << IR_RECEIVER_GPIO),

        .mode =
            GPIO_MODE_INPUT,

        /*
         * Many IR receiver modules already provide the
         * appropriate output circuitry.
         *
         * If yours requires an external pull-up, we can
         * change this later.
         */
        .pull_up_en =
            GPIO_PULLUP_DISABLE,

        .pull_down_en =
            GPIO_PULLDOWN_DISABLE,

        /*
         * Receiver goes HIGH -> LOW when IR is detected.
         */
        .intr_type =
            GPIO_INTR_NEGEDGE
    };


    ESP_ERROR_CHECK(
        gpio_config(
            &receiver_config
        )
    );


    /* --------------------------------------------------------
     * STATUS LED
     * ------------------------------------------------------ */

    gpio_config_t status_led_config = {

        .pin_bit_mask =
            (1ULL << STATUS_LED_GPIO),

        .mode =
            GPIO_MODE_OUTPUT,

        .pull_up_en =
            GPIO_PULLUP_DISABLE,

        .pull_down_en =
            GPIO_PULLDOWN_DISABLE,

        .intr_type =
            GPIO_INTR_DISABLE
    };


    ESP_ERROR_CHECK(
        gpio_config(
            &status_led_config
        )
    );


    /*
     * Start fail-safe.
     *
     * LED stays on until we have enough valid measurements.
     */
    gpio_set_level(
        STATUS_LED_GPIO,
        1
    );


    /* --------------------------------------------------------
     * GPIO INTERRUPT SERVICE
     * ------------------------------------------------------ */

    esp_err_t result =
        gpio_install_isr_service(
            ESP_INTR_FLAG_IRAM
        );


    /*
     * ESP_ERR_INVALID_STATE means somebody already installed
     * the GPIO ISR service.
     *
     * That's okay.
     */
    if ((result != ESP_OK) &&
        (result != ESP_ERR_INVALID_STATE))
    {
        ESP_ERROR_CHECK(result);
    }


    ESP_ERROR_CHECK(
        gpio_isr_handler_add(
            IR_RECEIVER_GPIO,
            receiver_gpio_isr,
            NULL
        )
    );
}


void initialize_ir_curtain(void)
{
    ESP_LOGI(
        TAG,
        "Starting IR light curtain"
    );


    /*
     * Build our 19-cycle 38 kHz waveform.
     */
    build_ir_burst();


    /*
     * Configure detector input and status LED.
     */
    configure_gpio();


    /*
     * Configure RMT transmitter.
     */
    configure_rmt();


    ESP_LOGI(
        TAG,
        "TX GPIO: %d",
        IR_TX_GPIO
    );


    ESP_LOGI(
        TAG,
        "RX GPIO: %d",
        IR_RECEIVER_GPIO
    );


    ESP_LOGI(
        TAG,
        "Carrier: ~38.46 kHz"
    );


    ESP_LOGI(
        TAG,
        "Burst: %d cycles (~494 us)",
        IR_CARRIER_CYCLES
    );
}
