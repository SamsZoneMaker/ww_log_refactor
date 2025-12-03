/**
 * @file ww_log_modules.h
 * @brief Module definitions and dynamic switch for logging system
 * @date 2025-12-01
 *
 * This file defines:
 * - Module IDs (0-31) used by both string and encode modes
 * - Dynamic module mask for runtime filtering (32-bit, one bit per module)
 * - APIs to control which modules are enabled
 *
 * Module Assignment:
 *   0      = DEFAULT/SYS (default module for logs without specific module)
 *   1      = DEMO
 *   2      = TEST
 *   3      = APP
 *   4      = DRIVERS
 *   5      = BROM
 *   6-31   = Reserved for future use
 *
 * Dynamic Switch Usage:
 *   - Each bit in g_ww_log_module_mask controls one module
 *   - Bit 0 = Module 0, Bit 1 = Module 1, etc.
 *   - 0xFFFFFFFF = All modules enabled
 *   - 0x00000000 = All modules disabled
 *   - 0x0000001F = Modules 0-4 enabled (DEMO, TEST, APP, DRIVERS, BROM)
 *
 * Examples:
 *   // Enable all modules
 *   ww_log_set_module_mask(0xFFFFFFFF);
 *
 *   // Disable BROM module (bit 5)
 *   ww_log_set_module_mask(0xFFFFFFDF);
 *
 *   // Only enable DEFAULT and DRIVERS
 *   ww_log_set_module_mask((1 << 0) | (1 << 4));
 */

#ifndef WW_LOG_MODULES_H
#define WW_LOG_MODULES_H

#include "ww_log_config.h"

/* ========== Module ID Definitions ========== */

/**
 * Module IDs (0-31)
 * These IDs are used in both string and encode modes
 */
#define WW_LOG_MODULE_DEFAULT   0   /**< Default/System module */
#define WW_LOG_MODULE_DEMO      1   /**< DEMO module */
#define WW_LOG_MODULE_TEST      2   /**< TEST module */
#define WW_LOG_MODULE_APP       3   /**< APP module */
#define WW_LOG_MODULE_DRIVERS   4   /**< DRIVERS module */
#define WW_LOG_MODULE_BROM      5   /**< BROM module */

/* Reserved: 6-31 for future modules */

/**
 * Maximum number of modules (32)
 */
#define WW_LOG_MODULE_MAX       32

/* ========== Static Module Switches (Compile-time) ========== */

/**
 * Static module switches - compile-time enable/disable
 *
 * These switches determine whether a module's LOG code is compiled into the binary.
 * - If set to 0: All LOG calls for this module are compiled out (zero code size)
 * - If set to 1: LOG code is compiled in and can be controlled by dynamic switch
 *
 * Use cases:
 * - Set to 0 to permanently exclude debug modules from production builds
 * - Set to 0 for unused modules to reduce binary size
 * - Keep at 1 for modules that need runtime control
 *
 * Configuration priority:
 *   1. Command line: -DWW_LOG_STATIC_MODULE_DEMO_EN=0
 *   2. ww_log_config.h or build system
 *   3. Default: 1 (enabled) if not defined
 *
 * Example:
 *   // In Makefile or build config
 *   CFLAGS += -DWW_LOG_STATIC_MODULE_TEST_EN=0    # Exclude TEST module
 *   CFLAGS += -DWW_LOG_STATIC_MODULE_DRIVERS_EN=0 # Exclude DRIVERS module
 */

#ifndef WW_LOG_STATIC_MODULE_DEFAULT_EN
#define WW_LOG_STATIC_MODULE_DEFAULT_EN   1  /**< DEFAULT module static switch */
#endif

#ifndef WW_LOG_STATIC_MODULE_DEMO_EN
#define WW_LOG_STATIC_MODULE_DEMO_EN      1  /**< DEMO module static switch */
#endif

#ifndef WW_LOG_STATIC_MODULE_TEST_EN
#define WW_LOG_STATIC_MODULE_TEST_EN      1  /**< TEST module static switch */
#endif

#ifndef WW_LOG_STATIC_MODULE_APP_EN
#define WW_LOG_STATIC_MODULE_APP_EN       1  /**< APP module static switch */
#endif

#ifndef WW_LOG_STATIC_MODULE_DRIVERS_EN
#define WW_LOG_STATIC_MODULE_DRIVERS_EN   1  /**< DRIVERS module static switch */
#endif

#ifndef WW_LOG_STATIC_MODULE_BROM_EN
#define WW_LOG_STATIC_MODULE_BROM_EN      1  /**< BROM module static switch */
#endif

/**
 * Module ID to static switch mapping
 * Used by LOG macros to check compile-time enable status
 */
#define WW_LOG_STATIC_ENABLED_0  WW_LOG_STATIC_MODULE_DEFAULT_EN
#define WW_LOG_STATIC_ENABLED_1  WW_LOG_STATIC_MODULE_DEMO_EN
#define WW_LOG_STATIC_ENABLED_2  WW_LOG_STATIC_MODULE_TEST_EN
#define WW_LOG_STATIC_ENABLED_3  WW_LOG_STATIC_MODULE_APP_EN
#define WW_LOG_STATIC_ENABLED_4  WW_LOG_STATIC_MODULE_DRIVERS_EN
#define WW_LOG_STATIC_ENABLED_5  WW_LOG_STATIC_MODULE_BROM_EN

/**
 * Static switch check macro with two-level expansion
 * This allows module_id to be a macro that expands to a number
 *
 * Example expansion:
 *   WW_LOG_STATIC_CHECK(WW_LOG_MODULE_DEMO)
 *   → WW_LOG_STATIC_CHECK(1)
 *   → WW_LOG_STATIC_ENABLED_1
 *   → WW_LOG_STATIC_MODULE_DEMO_EN
 *   → 1 (or 0 if disabled)
 */
#define WW_LOG_STATIC_CHECK(module_id)  _WW_LOG_STATIC_CHECK_IMPL(module_id)
#define _WW_LOG_STATIC_CHECK_IMPL(id)   WW_LOG_STATIC_ENABLED_##id

/* ========== Dynamic Module Mask ========== */

/**
 * Global module mask variable
 * Each bit controls whether a module is enabled
 * Default: 0xFFFFFFFF (all modules enabled)
 */
extern U32 g_ww_log_module_mask;

/**
 * @brief Check if a module is enabled
 * @param module_id Module ID (0-31)
 * @return Non-zero if enabled, 0 if disabled
 */
#define WW_LOG_MODULE_ENABLED(module_id) \
    ((g_ww_log_module_mask & (1U << (module_id))) != 0)

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
