/**
 * @file demo_process.c
 * @brief Demo module processing
 * @date 2025-11-29
 *
 * Example: File with custom offset for differentiation
 */

#include "demo_in.h"

/**
 * @brief Process a task
 * @param task_id Task identifier
 */
void demo_process(int task_id)
{
    /* File ID is automatically injected by Makefile via -DCURRENT_FILE_ID=xxx */
    LOG_DBG("Processing task...");

    if (task_id < 0) {
        LOG_ERR("Invalid task ID!");
        return;
    }

    LOG_INF("Task started, id=%d", task_id);

    /* Simulate task processing */
    int result = task_id * 2;

    /* Demonstrate different log levels */
    if (result > 100) {
        LOG_WRN("Result is large, id=%d, result=%d", task_id, result);
    }

    LOG_INF("Task completed, id=%d, result=%d", task_id, result);
}
