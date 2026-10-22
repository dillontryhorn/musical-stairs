#ifndef TRANSMIT_H
#define TRANSMIT_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_attr.h"

void build_ir_burst(void);
void configure_rmt(void);
void receiver_gpio_isr(void *arg);
bool transmit_ir_burst(void);
bool ir_was_detected(void);
uint32_t get_rmt_timeout_count(void);

#endif /* TRANSMIT_H */
