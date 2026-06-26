/**
 * @file n_ww_log_task_sim.c
 * @brief Sim replacement for log/n_ww_log_task.c.
 *
 * The original file cannot be compiled as-is because it:
 *   1. Contains a bare "#include <>" (syntax error, placeholder artifact)
 *   2. Includes FreeRTOS.h, semphr.h, timers.h with real FreeRTOS usage
 *
 * In the single-threaded PC sim, the mutex is a no-op: lock always succeeds
 * and unlock does nothing.  log_flush_task_init / log_flush_notify are also
 * provided as no-ops because CONFIG_N_LOG_BACKEND_EXT_MEM is off in the sim.
 */

#include "autoconf.h"       /* must be first: CONFIG_N_LOG_BACKEND_RAM guard */
#include "ww_std.h"
#include "log/n_ww_log_task.h"

#ifdef CONFIG_N_LOG_BACKEND_RAM

/* ====================================================================== */
/* Mutex — no-op in single-threaded sim                                    */
/* ====================================================================== */

WW_RTN log_mutex_lock(void)
{
    return WW_OK;
}

void log_mutex_lock_wait(void)
{
    /* no-op */
}

void log_mutex_unlock(void)
{
    /* no-op */
}

/* ====================================================================== */
/* Flush task — no-op in sim (ext-mem backend is disabled)                 */
/* ====================================================================== */

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

WW_RTN log_flush_task_init(void)
{
    return WW_OK;
}

void log_flush_notify(void)
{
    /* no-op */
}

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */

#endif /* CONFIG_N_LOG_BACKEND_RAM */
