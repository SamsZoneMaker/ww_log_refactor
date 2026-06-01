/**
 * @file demo_init.c
 * @brief Demo module initialization
 * @date 2025-11-29
 *
 * Example: File with custom offset for differentiation
 */

#include "demo_in.h"

/**
 * @brief Initialize demo module
 */
void demo_init(void)
{
    /* String mode: outputs [INF] demo_init.c:line - message */
    /* Encode mode: encodes with module_id, file_id, line, level */
    LOG_INF("Demo module initializing...");

    /* Simulate initialization steps */
    int status = 0;

    LOG_DBG("Checking hardware...");

    if (status == 0) {
        LOG_INF("Hardware check passed, code=%d", status);
    } else {
        LOG_ERR("Hardware check failed!");
    }

    LOG_WRN("Demo init completed with warnings, total=%d, failed=%d", 5, 1);
}
