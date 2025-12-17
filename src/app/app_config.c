/**
 * @file app_config.c
 * @brief Application configuration module
 * @date 2025-11-29
 */

#define CURRENT_FILE_OFFSET  APP_FILE_CONFIG
#include "app_in.h"

void app_config_load(void)
{
    LOG_INF(CURRENT_LOG_PARAM, "Loading configuration...");

    LOG_DBG(CURRENT_LOG_PARAM, "Reading config file...");
    LOG_INF(CURRENT_LOG_PARAM, "Configuration loaded successfully");
}

void app_config_save(void)
{
    LOG_INF(CURRENT_LOG_PARAM, "Saving configuration...");

    LOG_DBG(CURRENT_LOG_PARAM, "Writing config file...");
    LOG_INF(CURRENT_LOG_PARAM, "Configuration saved successfully");
}
