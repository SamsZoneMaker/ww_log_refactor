/**
 * @file demo_init.c
 * @brief Demo module initialization
 * @date 2025-11-29
 *
 * Example: File with custom offset for differentiation
 */

/* Define file offset BEFORE including module header */
#define CURRENT_FILE_OFFSET  DEMO_FILE_INIT

#include "demo_in.h"

/**
 * @brief Initialize demo module
 */
void demo_init(void)
{
    /* String mode: outputs [DEMO][INF][demo_init.c:line] message */
    /* Encode mode: encodes as LOG_ID=33 (32+1), LINE=__LINE__, LEVEL=2 */
    LOG_INF(CURRENT_LOG_PARAM, "Demo module initializing...");

    /* Simulate initialization steps */
    int status = 0;

    LOG_DBG(CURRENT_LOG_PARAM, "Checking hardware...");

    if (status == 0) {
        LOG_INF(CURRENT_LOG_PARAM, "Hardware check passed, code=%d", status);
    } else {
        LOG_ERR(CURRENT_LOG_PARAM, "Hardware check failed!");
    }

    LOG_WRN(CURRENT_LOG_PARAM, "Demo init completed with warnings, total=%d, failed=%d", 5, 1);
}
