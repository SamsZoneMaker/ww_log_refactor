/**
 * @file test_unit.c
 * @brief Unit test module
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_TEST_UNIT
#define CURRENT_MODULE_ID WW_LOG_MOD_TEST

/**
 * @brief Run unit tests
 */
void test_unit_run(void)
{
    TEST_LOG_INF_MSG("Starting unit tests...");

    int passed = 0;
    int failed = 0;

    /* Test 1 */
    TEST_LOG_DBG_MSG("Running test case 1...");
    if (1 + 1 == 2) {
        passed++;
        TEST_LOG_DBG_MSG("Test case 1 passed");
    } else {
        failed++;
        TEST_LOG_ERR_MSG("Test case 1 failed!");
    }

    /* Test 2 */
    TEST_LOG_DBG_MSG("Running test case 2...");
    if (2 * 3 == 6) {
        passed++;
    } else {
        failed++;
        TEST_LOG_ERR_MSG("Test case 2 failed!");
    }

    TEST_LOG_INF_MSG("Unit tests completed, passed=%d, failed=%d", passed, failed);

    if (failed > 0) {
        TEST_LOG_WRN_MSG("Some tests failed!");
    }
}
