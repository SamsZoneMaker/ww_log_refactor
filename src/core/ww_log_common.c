/**
 * @file ww_log_common.c
 * @brief Common log system utilities
 * @date 2025-11-18
 */

#include "ww_log.h"

/* ========== Global Variables ========== */

/**
 * Dynamic module enable/disable switches
 * Default: all enabled (can be changed at runtime)
 */
U8 g_ww_log_mod_enable[WW_LOG_MOD_MAX] = {
    1,  /* WW_LOG_MOD_DEMO */
    1,  /* WW_LOG_MOD_TEST */
    1,  /* WW_LOG_MOD_APP */
    1,  /* WW_LOG_MOD_DRIVERS */
    1,  /* WW_LOG_MOD_BROM */
};

/**
 * Global dynamic log level threshold
 * Default: DBG (allow all levels)
 */
U8 g_ww_log_level_threshold = WW_LOG_LEVEL_DBG;

/* ========== RAM Buffer (encode_mode only) ========== */

#if defined(CONFIG_WW_LOG_ENCODE_MODE) && defined(CONFIG_WW_LOG_RAM_BUFFER_EN)

WW_LOG_RAM_T g_ww_log_ram = {
    .magic = 0,
    .head = 0,
    .tail = 0,
};

#endif

/* ========== Function Implementations ========== */

/**
 * @brief Initialize log system
 */
void ww_log_init(void)
{
    /* Initialize module enable array (already done in global init) */
    /* All modules enabled by default */

    /* Initialize log level threshold */
    g_ww_log_level_threshold = CONFIG_WW_LOG_LEVEL_THRESHOLD;

#if defined(CONFIG_WW_LOG_ENCODE_MODE) && defined(CONFIG_WW_LOG_RAM_BUFFER_EN)
    /* Check for warm restart (magic number detection) */
    if (g_ww_log_ram.magic != WW_LOG_RAM_MAGIC) {
        /* Cold start or first initialization */
        g_ww_log_ram.magic = WW_LOG_RAM_MAGIC;
        g_ww_log_ram.head = 0;
        g_ww_log_ram.tail = 0;

        /* Clear buffer contents */
        for (U16 i = 0; i < CONFIG_WW_LOG_RAM_ENTRY_NUM; i++) {
            g_ww_log_ram.entries[i] = 0;
        }
    } else {
        /* Warm restart detected - preserve existing logs */
        /* head and tail pointers remain unchanged */
        /* This allows reading logs that survived the reset */
    }
#endif

#ifdef CONFIG_WW_LOG_OUTPUT_UART
    /* UART initialization (if needed) */
    /* In this test environment, we use stdout which is already available */
#endif
}

/**
 * @brief Check if module logging is enabled
 * @param module_id Module ID
 * @return 1 if enabled, 0 if disabled
 */
U8 ww_log_is_mod_enabled(WW_LOG_MODULE_E module_id)
{
    if (module_id >= WW_LOG_MOD_MAX) {
        return 0;  /* Invalid module ID */
    }

    /* Check static compile-time switch first */
    switch (module_id) {
#ifndef CONFIG_WW_LOG_MOD_DEMO_EN
        case WW_LOG_MOD_DEMO:
            return 0;
#endif
#ifndef CONFIG_WW_LOG_MOD_TEST_EN
        case WW_LOG_MOD_TEST:
            return 0;
#endif
#ifndef CONFIG_WW_LOG_MOD_APP_EN
        case WW_LOG_MOD_APP:
            return 0;
#endif
#ifndef CONFIG_WW_LOG_MOD_DRIVERS_EN
        case WW_LOG_MOD_DRIVERS:
            return 0;
#endif
#ifndef CONFIG_WW_LOG_MOD_BROM_EN
        case WW_LOG_MOD_BROM:
            return 0;
#endif
        default:
            break;
    }

    /* Check dynamic runtime switch */
    return g_ww_log_mod_enable[module_id];
}

/**
 * @brief Check if log level is enabled
 * @param level Log level
 * @return 1 if enabled, 0 if disabled
 */
U8 ww_log_is_level_enabled(WW_LOG_LEVEL_E level)
{
    /* Check global compile-time threshold */
#if CONFIG_WW_LOG_LEVEL_THRESHOLD < WW_LOG_LEVEL_DBG
    if (level == WW_LOG_LEVEL_DBG) {
        return 0;
    }
#endif
#if CONFIG_WW_LOG_LEVEL_THRESHOLD < WW_LOG_LEVEL_INF
    if (level == WW_LOG_LEVEL_INF) {
        return 0;
    }
#endif
#if CONFIG_WW_LOG_LEVEL_THRESHOLD < WW_LOG_LEVEL_WRN
    if (level == WW_LOG_LEVEL_WRN) {
        return 0;
    }
#endif

    /* Check dynamic runtime threshold */
    return (level <= g_ww_log_level_threshold);
}
