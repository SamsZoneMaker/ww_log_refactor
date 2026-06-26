/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* to add description for this file if needed */
/*************************** description end *****************************/

// #include <>
// #include ""
#include "ww_std.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"

#include "log/n_ww_log_task.h"

#ifdef CONFIG_N_LOG_BACKEND_RAM

/*************************** global variable start ***************************/
/* to be used in all files */
/*************************** global variable end *****************************/

/*************************** macro definition start ***************************/
/* to be used only in this file */

/* Mutex acquire timeout for the (RAM-backend) writer path. Lives outside the
 * EXT_MEM guard because log_mutex_lock() is compiled whenever RAM is on. */
#define LOG_WRITE_TIMEOUT_MS         (6)

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

#define LOG_FLUSH_TASK_STACK_SIZE    (256)
#define LOG_FLUSH_TASK_PRIORITY      (1)
#define LOG_FLUSH_TIMEOUT_MS         (10000)

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */
/*************************** macro definition end *****************************/

/*************************** type definition start ***************************/
/* to be used only in this file */
/*************************** type definition end *****************************/

/*************************** declaration start ***************************/
/* to be used only in this file */
#ifdef CONFIG_N_LOG_BACKEND_RAM

extern int log_ram_flush(void);
extern int log_ext_mem_is_full(void);
extern U16 log_ram_get_pending_len(void);

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */
/*************************** declaration end *****************************/

/*************************** static variable start ***************************/
/* to be used only in this file */
#ifdef CONFIG_N_LOG_BACKEND_RAM

static TaskHandle_t      g_flush_task_handle = NULL;
static SemaphoreHandle_t g_flush_semaphore = NULL;
static SemaphoreHandle_t g_log_mutex = NULL;

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */
/*************************** static variable end *****************************/

/*************************** static function start ***************************/
/* to be used only in this file */
#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

/**
 * @brief The entry function for the flush task
 * * @note
 * - Waiting for semaphore
 * - If receive the semaphore, execute log_ram_flush()
 * - Continue looping and wait for the next signal
 */
static void log_flush_task(void *pvParameters)
{
    (void)pvParameters;

    const TickType_t flush_timeout = pdMS_TO_TICKS(LOG_FLUSH_TIMEOUT_MS);

    ww_printf("LOG_FLUSH_TASK: Started\n");

    while(1)
    {
        if (xSemaphoreTake(g_flush_semaphore, flush_timeout) == pdTRUE)
        {
            if (log_ext_mem_is_full())
            {
                continue; // if extmem is full, skip flushing, ring buffer in ram
            }

            /* Check if there is any data that needs to be flushed. */
            U32 pending = log_ram_get_pending_len();

            if (pending > 0)
            {
                ww_printf("LOG_FLUSH_TASK: Flushing, pending_len = %u\n", pending);

                int ret = log_ram_flush();

                if (ret != WW_OK)
                {
                    ww_printf("LOG_FLUSH_TASK: Flush failed, ret = %d\n", ret);
                }
            }
        }
    }
}

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */
/*************************** static function end *****************************/


/*************************** global function start ***************************/
#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

WW_RTN log_flush_task_init(void)
{
    BaseType_t ret;

    ww_printf("LOG_FLUSH_TASK: Initializing ... \n");

    /* ---------------------------------------------------
     * Step 1: Initialize a semaphore
     * --------------------------------------------------- */
    g_flush_semaphore = xSemaphoreCreateBinary();
    if (g_flush_semaphore == NULL)
    {
        ww_printf("LOG_FLUSH_TASK: Failed to create semaphore\n");
        return WW_ERR;
    }

    /* ---------------------------------------------------
     * Step 2: Ensure the mutex exists. Normally created by log_lock_init()
     *         from n_ww_log_init(); create here too as a safety net.
     * --------------------------------------------------- */
    if (log_lock_init() != WW_OK)
    {
        ww_printf("LOG_FLUSH_TASK: Failed to create mutex\n");
        vSemaphoreDelete(g_flush_semaphore);
        g_flush_semaphore = NULL;
        return WW_ERR;
    }

    /* ---------------------------------------------------
     * Step 3: Create a flush task
     * --------------------------------------------------- */
    ret = xTaskCreate(log_flush_task,
                      "LogFlush",
                      LOG_FLUSH_TASK_STACK_SIZE,
                      NULL,
                      LOG_FLUSH_TASK_PRIORITY,
                      &g_flush_task_handle);

    if (ret != pdPASS)
    {
        ww_printf("LOG_FLUSH_TASK: Failed to create task\n");
        vSemaphoreDelete(g_flush_semaphore);
        g_flush_semaphore = NULL;
        return WW_ERR;
    }

    ww_printf("LOG_FLUSH_TASK: Initialized successfully\n");
    return WW_OK;
}

void log_flush_notify(void)
{
    if (g_flush_semaphore != NULL)
    {
        xSemaphoreGive(g_flush_semaphore);
    }
}

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */

/**
 * @brief Create the log mutex if it does not already exist.
 *        Idempotent: safe to call from both n_ww_log_init() and
 *        log_flush_task_init().
 */
WW_RTN log_lock_init(void)
{
    if (g_log_mutex != NULL)
    {
        return WW_OK;   /* already created */
    }
    g_log_mutex = xSemaphoreCreateMutex();
    if (g_log_mutex == NULL)
    {
        return WW_ERR;
    }
    return WW_OK;
}

WW_RTN log_mutex_lock(void)
{
    if (g_log_mutex == NULL)
    {
        return WW_OK;
    }
    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(LOG_WRITE_TIMEOUT_MS)) == pdTRUE)
    {
        return WW_OK;
    }

    /* BUGFIX: previously returned WW_FALSE (==0 == WW_OK) so the caller's
     * "if (lock() != WW_OK)" never detected a timeout. Return WW_ERR. */
    return WW_ERR;
}

void log_mutex_lock_wait(void)
{
    if (g_log_mutex == NULL)
    {
        return;
    }
    xSemaphoreTake(g_log_mutex, portMAX_DELAY);
}

void log_mutex_unlock(void)
{
    if (g_log_mutex == NULL)
    {
        return;
    }
    xSemaphoreGive(g_log_mutex);
}

/*************************** global function end *****************************/

#endif /* CONFIG_LOG_OUTPUT_TO_RAM */
