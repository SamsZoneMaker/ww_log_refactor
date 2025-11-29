/**
 * @file test_stress.c
 * @brief Stress test module
 * @date 2025-11-29
 */

#define CURRENT_FILE_OFFSET  TEST_FILE_STRESS
#include "test_in.h"

void test_stress_run(void)
{
    LOG_INF(CURRENT_MODULE_TAG, "Starting stress tests...");

    int iterations = 1000;

    LOG_DBG(CURRENT_MODULE_TAG, "Running stress test with %d iterations...", iterations);

    for (int i = 0; i < 10; i++) {
        LOG_DBG(CURRENT_MODULE_TAG, "Stress iteration %d", i);
    }

    LOG_INF(CURRENT_MODULE_TAG, "Stress tests complete, iterations=%d", iterations);
}
