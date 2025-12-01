/**
 * @file demo_process.c
 * @brief Demo module processing
 * @date 2025-11-29
 *
 * Example: File with custom offset for differentiation
 */

/* Define file offset BEFORE including module header */
#define CURRENT_FILE_OFFSET  DEMO_FILE_PROCESS

#include "demo_in.h"

/**
 * @brief Process demo tasks
 */
void demo_process(int task_id)
{
    /* String mode: outputs [DEMO][DBG][demo_process.c:line] message */
    /* Encode mode: encodes as LOG_ID=34 (32+2), LINE=__LINE__, LEVEL=3 */
    LOG_DBG(CURRENT_LOG_PARAM, "Processing task...");

    if (task_id < 0) {
        LOG_ERR(CURRENT_LOG_PARAM, "Invalid task ID!");
        return;
    }

    LOG_INF(CURRENT_LOG_PARAM, "Task started, id=%d", task_id);

    /* Simulate processing */
    int result = task_id * 2;
#ifdef WW_LOG_MODE_ENCODE
    (void)result;  /* Suppress unused warning in encode mode */
#endif

    LOG_INF(CURRENT_LOG_PARAM, "Task completed, id=%d, result=%d", task_id, result);
}
