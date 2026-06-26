/**
 * @file demo_init.c
 * @brief Demo module: initialisation (registered in log_config.json).
 *
 * Uses the v2 log API: N_LOG_ERR / N_LOG_WRN / N_LOG_INF / N_LOG_DBG.
 */

#include "log/n_ww_log.h"
#include "demo_in.h"

void demo_init(void)
{
    N_LOG_INF("Demo module initializing...");
    N_LOG_DBG("Hardware setup started");
    N_LOG_INF("Hardware check passed, code=%d", 0);
    N_LOG_WRN("Demo init warning, val=%d", 42);
}
