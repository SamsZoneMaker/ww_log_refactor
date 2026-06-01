/**
 * @file test_unit.c
 * @brief Unit test cases
 * @date 2025-11-29
 */

#include "test_in.h"

void test_unit_run(void)
{
    /* File ID is automatically injected by Makefile via -DCURRENT_FILE_ID=xxx */
    LOG_INF("Starting unit tests...");

    int passed = 0;
    int failed = 0;

    /* Test 1 */
    LOG_DBG("Running test case 1...");
    if (1 + 1 == 2) {
        passed++;
        LOG_DBG("Test case 1 passed");
    } else {
        failed++;
        LOG_ERR("Test case 1 failed!");
    }

    /* Test 2 */
    LOG_DBG("Running test case 2...");
    if (10 / 2 == 5) {
        passed++;
        LOG_DBG("Test case 2 passed");
    } else {
        failed++;
        LOG_ERR("Test case 2 failed!");
    }

    LOG_INF("Unit tests complete, passed=%d, failed=%d", passed, failed);

    if (failed > 0) {
        LOG_WRN("Some tests failed!");
    }
}
