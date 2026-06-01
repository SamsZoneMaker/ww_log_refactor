/**
 * @file ww_log_modules.h
 * @brief Module definitions and dynamic switch for logging system
 * @date 2025-12-01
 *
 * This file provides:
 * - Dynamic module mask for runtime filtering (32-bit, one bit per module)
 * - APIs to control which modules are enabled at runtime
 * - Level threshold control for runtime log filtering
 *
 * Module configuration is centralized in log_config.json:
 * - Module IDs and names are auto-generated in auto_file_ids.h
 * - Static compile-time switches are auto-generated in auto_file_ids.h
 * - File IDs are auto-generated in auto_file_ids.h
 *
 * File ID Assignment (64 files per module):
 *   - File ID = base_id + offset, where base_id = module_id * 64
 *   - Module 0 (DEFAULT): File IDs 0-63
 *   - Module 1 (DEMO):    File IDs 64-127
 *   - Module 2 (TEST):    File IDs 128-191
 *   - Module 3 (APP):     File IDs 192-255
 *   - Module 4 (DRIVERS): File IDs 256-319
 *   - Module 5 (BROM):    File IDs 320-383
 *
 * Dynamic Switch Usage:
 *   - Each bit in g_ww_log_module_mask controls one module
 *   - Bit 0 = Module 0, Bit 1 = Module 1, etc.
 *   - 0xFFFFFFFF = All modules enabled
 *   - 0x00000000 = All modules disabled
 *
 * Examples:
 *   // Enable all modules
 *   ww_log_set_module_mask(0xFFFFFFFF);
 *
 *   // Disable BROM module (bit 5)
 *   ww_log_set_module_mask(0xFFFFFFDF);
 *
 *   // Only enable DEFAULT and DRIVERS
 *   ww_log_set_module_mask((1 << WW_LOG_MODULE_DEFAULT) | (1 << WW_LOG_MODULE_DRIVERS));
 */

#ifndef WW_LOG_MODULES_H
#define WW_LOG_MODULES_H

#include "type.h"
#include "auto_file_ids.h"  /* Auto-generated module IDs and static switches */

/* ========== Dynamic Module Mask ========== */

/**
 * Global module mask variable
 * Each bit controls whether a module is enabled at runtime
 * Default: 0xFFFFFFFF (all modules enabled)
 */
extern U32 g_ww_log_module_mask;

/**
 * @brief Check if a module is enabled (runtime check)
 * @param module_id Module ID (0-31)
 * @return Non-zero if enabled, 0 if disabled
 * #define WW_LOG_MODULE_ENABLED(module_id) \
 *    ((g_ww_log_module_mask & (1U << (module_id))) != 0)
*/
/* ========== API Functions ========== */

/**
 * @brief Set module mask to control which modules are enabled
 * @param mask 32-bit mask, each bit controls one module
 *
 * Examples:
 *   ww_log_set_module_mask(0xFFFFFFFF);  // Enable all
 *   ww_log_set_module_mask(0x00000000);  // Disable all
 *   ww_log_set_module_mask(0x0000003F);  // Enable modules 0-5
 */
void ww_log_set_module_mask(U32 mask);

/**
 * @brief Get current module mask
 * @return Current 32-bit module mask
 */
U32 ww_log_get_module_mask(void);

/**
 * @brief Enable a specific module
 * @param module_id Module ID (0-31)
 */
void ww_log_enable_module(U8 module_id);

/**
 * @brief Disable a specific module
 * @param module_id Module ID (0-31)
 */
void ww_log_disable_module(U8 module_id);

/**
 * @brief Check if a specific module is enabled
 * @param module_id Module ID (0-31)
 * @return 1 if enabled, 0 if disabled
 */
U8 ww_log_is_module_enabled(U8 module_id);

/* ========== Level Threshold Control ========== */

/**
 * Global log level threshold (runtime configurable)
 * Logs with level > threshold will be filtered out
 * Default: WW_LOG_LEVEL_DBG (allow all)
 */
extern U8 g_ww_log_level_threshold;

/**
 * @brief Set global log level threshold
 * @param level New threshold (WW_LOG_LEVEL_ERR/WRN/INF/DBG)
 *
 * Examples:
 *   ww_log_set_level_threshold(WW_LOG_LEVEL_ERR);  // Only errors
 *   ww_log_set_level_threshold(WW_LOG_LEVEL_WRN);  // Errors + warnings
 *   ww_log_set_level_threshold(WW_LOG_LEVEL_DBG);  // All logs
 */
void ww_log_set_level_threshold(U8 level);

/**
 * @brief Get current log level threshold
 * @return Current threshold value
 */
U8 ww_log_get_level_threshold(void);

#endif /* WW_LOG_MODULES_H */
