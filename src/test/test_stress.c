/**
 * @file test_stress.c
 * @brief Stress test module
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_TEST_STRESS
#define CURRENT_MODULE_ID WW_LOG_MOD_TEST

/**
 * @brief Run stress tests
 */
void test_stress_run(void)
{
    TEST_LOG_INF_MSG("Starting stress tests...");

    int iterations = 100;
    int errors = 0;

    TEST_LOG_DBG_MSG("Running stress test iterations...");

    for (int i = 0; i < 10; i++) {
        if (i % 3 == 0) {
            errors++;
            TEST_LOG_WRN_MSG("Stress test warning at iteration, i=%d", i);
        }
    }

    if (errors > 5) {
        TEST_LOG_ERR_MSG("Too many errors in stress test!");
    }

    TEST_LOG_INF_MSG("Stress test completed, iterations=%d, errors=%d",
                     iterations, errors);
}
