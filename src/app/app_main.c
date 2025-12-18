/**
 * @file app_main.c
 * @brief Application main module
 * @date 2025-11-29
 */

#include "app_in.h"

void app_main(void)
{
    /* File ID is automatically injected by Makefile via -DCURRENT_FILE_ID=xxx */
    LOG_INF("Application starting...");

    int app_version = 100;

    /* Simulate initialization */

    LOG_DBG("Initializing application subsystems...");
    LOG_INF("Application initialized, version=%d", app_version);
}

void app_shutdown(void)
{
    LOG_INF("Application shutting down...");
    LOG_DBG("Cleaning up resources...");
    LOG_INF("Application shutdown complete");
}
