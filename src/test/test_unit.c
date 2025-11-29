/**
 * @file test_unit.c
 * @brief Unit test module
 * @date 2025-11-29
 */

/* Use custom file offset for differentiation */
#define CURRENT_FILE_OFFSET  TEST_FILE_UNIT
#include "test_in.h"

/**
 * @brief Run unit tests
 */
void test_unit_run(void)
{
    LOG_INF(CURRENT_MODULE_TAG, "Starting unit tests...");

    int passed = 0;
    int failed = 0;

    /* Test 1 */
    LOG_DBG(CURRENT_MODULE_TAG, "Running test case 1...");
    if (1 + 1 == 2) {
        passed++;
        LOG_DBG(CURRENT_MODULE_TAG, "Test case 1 passed");
    } else {
        failed++;
        LOG_ERR(CURRENT_MODULE_TAG, "Test case 1 failed!");
    }

    /* Test 2 */
    LOG_DBG(CURRENT_MODULE_TAG, "Running test case 2...");
    if (10 / 2 == 5) {
        passed++;
        LOG_DBG(CURRENT_MODULE_TAG, "Test case 2 passed");
    } else {
        failed++;
        LOG_ERR(CURRENT_MODULE_TAG, "Test case 2 failed!");
    }

    LOG_INF(CURRENT_MODULE_TAG, "Unit tests complete, passed=%d, failed=%d", passed, failed);

    if (failed > 0) {
        LOG_WRN(CURRENT_MODULE_TAG, "Some tests failed!");
    }
}
