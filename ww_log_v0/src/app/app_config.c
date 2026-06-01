/**
 * @file app_config.c
 * @brief Application configuration management
 * @date 2025-11-29
 */

#include "app_in.h"

void app_config_load(void)
{
    /* File ID is automatically injected by Makefile via -DCURRENT_FILE_ID=xxx */
    LOG_INF("Loading configuration...");

    LOG_DBG("Reading config file...");
    LOG_INF("Configuration loaded successfully");
}

void app_config_save(void)
{
    LOG_INF("Saving configuration...");

    LOG_DBG("Writing config file...");
    LOG_INF("Configuration saved successfully");
}
