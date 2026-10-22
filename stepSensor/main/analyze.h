#ifndef ANALYZE_H
#define ANALYZE_H

#include <stdbool.h>

/* ============================================================
 * TYPES
 * ============================================================ */

typedef enum
{
    CURTAIN_STARTING = 0,
    CURTAIN_CLEAR,
    CURTAIN_BLOCKED,
    CURTAIN_INVALID

} curtain_status_t;


void set_curtain_status(curtain_status_t new_status);
void process_measurement(int baseline, bool saw_ir);
curtain_status_t get_curtain_status(void);

#endif /* ANALYZE_H */
