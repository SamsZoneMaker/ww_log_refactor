/**
 * @file demo_init.c
 * @brief Demo module initialization.
 */

#include "demo_in.h"

void demo_init(void)
{
    LOG_INF("Demo module initializing...");

    int status = 0;

    LOG_DBG("Checking hardware...");

    if (status == 0) {
        LOG_INF("Hardware check passed, code=%d", status);
    } else {
        LOG_ERR("Hardware check failed!");
    }

    LOG_WRN("Demo init completed with warnings, total=%d, failed=%d", 5, 1);
}
