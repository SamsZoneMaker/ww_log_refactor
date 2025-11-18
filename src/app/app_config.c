/**
 * @file app_config.c
 * @brief Application configuration module
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_APP_CONFIG
#define CURRENT_MODULE_ID WW_LOG_MOD_APP

/**
 * @brief Load application configuration
 */
void app_config_load(void)
{
    TEST_LOG_INF_MSG("Loading configuration...");

    int config_version = 1;
    int config_size = 256;

    TEST_LOG_DBG_MSG("Reading configuration file...");

    if (config_version < 1) {
        TEST_LOG_ERR_MSG("Invalid configuration version!");
        return;
    }

    TEST_LOG_INF_MSG("Configuration loaded, version=%d, size=%d",
                     config_version, config_size);

    if (config_size > 512) {
        TEST_LOG_WRN_MSG("Configuration size exceeds recommended limit");
    }
}

/**
 * @brief Save application configuration
 */
void app_config_save(void)
{
    TEST_LOG_DBG_MSG("Saving configuration...");

    int write_status = 0;

    if (write_status != 0) {
        TEST_LOG_ERR_MSG("Failed to save configuration!");
    } else {
        TEST_LOG_INF_MSG("Configuration saved successfully");
    }
}
