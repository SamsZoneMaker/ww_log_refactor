/**
 * @file demo_init.c
 * @brief Demo module initialization
 */

#include "ww_log.h"

/* Define current file ID and module ID */
#define CURRENT_FILE_ID   FILE_ID_DEMO_INIT
#define CURRENT_MODULE_ID WW_LOG_MOD_DEMO

/**
 * @brief Initialize demo module
 */
void demo_init(void)
{
    TEST_LOG_INF_MSG("Demo module initializing...");

    /* Simulate initialization steps */
    int status = 0;

    TEST_LOG_DBG_MSG("Checking hardware...");

    if (status == 0) {
        TEST_LOG_INF_MSG("Hardware check passed, code=%d", status);
    } else {
        TEST_LOG_ERR_MSG("Hardware check failed!");
    }

    TEST_LOG_WRN_MSG("Demo init completed with warnings, total_checks=%d, failed=%d", 5, 1);
}
