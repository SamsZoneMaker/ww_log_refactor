/**
 * @file app_main.c
 * @brief Application main module
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_APP_MAIN
#define CURRENT_MODULE_ID WW_LOG_MOD_APP

/**
 * @brief Application main function
 */
void app_main(void)
{
    TEST_LOG_INF_MSG("Application starting...");

    TEST_LOG_DBG_MSG("Initializing subsystems...");

    int subsystem_count = 3;
    int init_success = 1;

    if (init_success) {
        TEST_LOG_INF_MSG("All subsystems initialized, count=%d", subsystem_count);
    } else {
        TEST_LOG_ERR_MSG("Subsystem initialization failed!");
        return;
    }

    TEST_LOG_INF_MSG("Application running...");
}

/**
 * @brief Application shutdown
 */
void app_shutdown(void)
{
    TEST_LOG_INF_MSG("Application shutting down...");

    int cleanup_code = 0;

    TEST_LOG_INF_MSG("Cleanup completed, code=%d", cleanup_code);

    TEST_LOG_INF_MSG("Application terminated");
}
