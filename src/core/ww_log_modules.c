/**
 * @file ww_log_modules.c
 * @brief Module mask management implementation
 * @date 2025-12-01
 */

#include "ww_log_modules.h"

/* ========== Global Variables ========== */

/**
 * Module mask - each bit controls one module (0-31)
 * Default: 0xFFFFFFFF (all modules enabled)
 */
U32 g_ww_log_module_mask = 0xFFFFFFFF;

/* ========== API Implementation ========== */

/**
 * @brief Set module mask
 */
void ww_log_set_module_mask(U32 mask)
{
    g_ww_log_module_mask = mask;
}

/**
 * @brief Get current module mask
 */
U32 ww_log_get_module_mask(void)
{
    return g_ww_log_module_mask;
}

/**
 * @brief Enable a specific module
 */
void ww_log_enable_module(U8 module_id)
{
    if (module_id < WW_LOG_MODULE_MAX) {
        g_ww_log_module_mask |= (1U << module_id);
    }
}

/**
 * @brief Disable a specific module
 */
void ww_log_disable_module(U8 module_id)
{
    if (module_id < WW_LOG_MODULE_MAX) {
        g_ww_log_module_mask &= ~(1U << module_id);
    }
}

/**
 * @brief Check if a specific module is enabled
 */
U8 ww_log_is_module_enabled(U8 module_id)
{
    if (module_id >= WW_LOG_MODULE_MAX) {
        return 0;
    }
    return (g_ww_log_module_mask & (1U << module_id)) ? 1 : 0;
}
