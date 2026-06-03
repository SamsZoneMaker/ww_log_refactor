/**
 * @file demo_process.c
 * @brief Demo module processing.
 */

#include "demo_in.h"

void demo_process(int task_id)
{
    LOG_DBG("Processing task...");

    if (task_id < 0) {
        LOG_ERR("Invalid task ID!");
        return;
    }

    LOG_INF("Task started, id=%d", task_id);

    int result = task_id * 2;

    if (result > 100) {
        LOG_WRN("Result is large, id=%d, result=%d", task_id, result);
    }

    LOG_INF("Task completed, id=%d, result=%d", task_id, result);
}
