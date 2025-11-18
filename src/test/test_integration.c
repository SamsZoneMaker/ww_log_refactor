/**
 * @file test_integration.c
 * @brief Integration test module
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_TEST_INTEGRATION
#define CURRENT_MODULE_ID WW_LOG_MOD_TEST

/**
 * @brief Run integration tests
 */
void test_integration_run(void)
{
    TEST_LOG_INF_MSG("Starting integration tests...");

    TEST_LOG_DBG_MSG("Testing module interactions...");

    int module_a_ok = 1;
    int module_b_ok = 1;

    if (!module_a_ok) {
        TEST_LOG_ERR_MSG("Module A integration failed!");
    }

    if (!module_b_ok) {
        TEST_LOG_ERR_MSG("Module B integration failed!");
    }

    if (module_a_ok && module_b_ok) {
        TEST_LOG_INF_MSG("All modules integrated successfully");
    } else {
        TEST_LOG_WRN_MSG("Integration completed with errors, a_ok=%d, b_ok=%d",
                         module_a_ok, module_b_ok);
    }
}
