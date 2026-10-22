#include "initialization.h"
#include "measurement.h"
#include "step_sender.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    initialize_ir_curtain();
    step_sender_init();

    /* Start the measurement loop after hardware initialization. */
    ESP_ERROR_CHECK(xTaskCreate(
        ir_measurement_task,
        "ir_measurement",
        4096,
        NULL,
        10,
        NULL
    ) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
