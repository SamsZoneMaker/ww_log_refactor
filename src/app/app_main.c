/**
 * @file app_main.c
 * @brief Application main module
 * @date 2025-11-29
 */

#define CURRENT_FILE_OFFSET  APP_FILE_MAIN
#include "app_in.h"

void app_main(void)
{
    LOG_INF(CURRENT_MODULE_TAG, "Application starting...");

    int app_version = 100;
#ifdef WW_LOG_MODE_ENCODE
    (void)app_version;  /* Suppress unused warning in encode mode */
#endif

    LOG_DBG(CURRENT_MODULE_TAG, "Initializing application subsystems...");
    LOG_INF(CURRENT_MODULE_TAG, "Application initialized, version=%d", app_version);
}

void app_shutdown(void)
{
    LOG_INF(CURRENT_MODULE_TAG, "Application shutting down...");
    LOG_DBG(CURRENT_MODULE_TAG, "Cleaning up resources...");
    LOG_INF(CURRENT_MODULE_TAG, "Application shutdown complete");
}
