/**
 * @file demo_process.c
 * @brief Demo module processing
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_DEMO_PROCESS
#define CURRENT_MODULE_ID WW_LOG_MOD_DEMO

/**
 * @brief Process demo tasks
 */
void demo_process(int task_id)
{
    TEST_LOG_DBG_MSG("Processing task...");

    if (task_id < 0) {
        TEST_LOG_ERR_MSG("Invalid task ID!");
        return;
    }

    TEST_LOG_INF_MSG("Task started, id=%d", task_id);

    /* Simulate processing */
    int result = task_id * 2;

    TEST_LOG_INF_MSG("Task completed, id=%d, result=%d", task_id, result);
}
