/**
 * @file demo_process.c
 * @brief Demo module: processing loop (registered in log_config.json).
 */

#include "log/n_ww_log.h"
#include "demo_in.h"

/* Emit `n` INF entries (one U32 param each) for volume/stress testing the
 * RAM ring and the external-storage flush path. */
void demo_burst(int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        N_LOG_INF("burst i=%d v=%x", i, (unsigned)(0xB0000000u + (unsigned)i));
    }
}

void demo_process(int task_id)
{
    if (task_id < 0)
    {
        N_LOG_ERR("demo_process: invalid task_id=%d", task_id);
        return;
    }

    N_LOG_INF("demo_process: start, task_id=%d", task_id);
    N_LOG_DBG("demo_process: inner loop x=%d y=%d", task_id, task_id * 2);
    N_LOG_INF("demo_process: done, task_id=%d", task_id);
}
