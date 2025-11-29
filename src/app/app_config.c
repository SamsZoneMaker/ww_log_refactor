/**
 * @file app_config.c
 * @brief Application configuration module
 * @date 2025-11-29
 */

#define CURRENT_FILE_OFFSET  APP_FILE_CONFIG
#include "app_in.h"

void app_config_load(void)
{
    LOG_INF(CURRENT_MODULE_TAG, "Loading configuration...");

    LOG_DBG(CURRENT_MODULE_TAG, "Reading config file...");
    LOG_INF(CURRENT_MODULE_TAG, "Configuration loaded successfully");
}

void app_config_save(void)
{
    LOG_INF(CURRENT_MODULE_TAG, "Saving configuration...");

    LOG_DBG(CURRENT_MODULE_TAG, "Writing config file...");
    LOG_INF(CURRENT_MODULE_TAG, "Configuration saved successfully");
}
