/**
 * @file ww_log_str.c
 * @brief String mode implementation
 * @date 2025-11-18
 */

#include "ww_log.h"
#include <stdarg.h>
#include <string.h>

#ifdef CONFIG_WW_LOG_STR_MODE

/**
 * @brief Get log level string
 * @param level Log level
 * @return Level string (ERR/WRN/INF/DBG)
 */
static const char* ww_log_get_level_str(WW_LOG_LEVEL_E level)
{
    switch (level) {
        case WW_LOG_LEVEL_ERR: return "ERR";
        case WW_LOG_LEVEL_WRN: return "WRN";
        case WW_LOG_LEVEL_INF: return "INF";
        case WW_LOG_LEVEL_DBG: return "DBG";
        default: return "???";
    }
}

/**
 * @brief Get module name string
 * @param module_id Module ID
 * @return Module name
 */
static const char* ww_log_get_module_str(WW_LOG_MODULE_E module_id)
{
    switch (module_id) {
        case WW_LOG_MOD_DEMO:    return "DEMO";
        case WW_LOG_MOD_TEST:    return "TEST";
        case WW_LOG_MOD_APP:     return "APP";
        case WW_LOG_MOD_DRIVERS: return "DRV";
        case WW_LOG_MOD_BROM:    return "BROM";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Extract filename from full path
 * @param path Full file path
 * @return Pointer to filename (without path)
 */
static const char* ww_log_extract_filename(const char *path)
{
    const char *filename = strrchr(path, '/');
    if (filename == NULL) {
        filename = strrchr(path, '\\');  /* Windows path separator */
    }
    return (filename != NULL) ? (filename + 1) : path;
}

/**
 * @brief String mode logging function
 * @param module_id Module ID
 * @param level Log level
 * @param filename Source filename (with path)
 * @param line Line number
 * @param fmt Format string
 */
void ww_log_str_output(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                       const char *filename, U16 line, const char *fmt, ...)
{
    /* Check if module is enabled */
    if (!ww_log_is_mod_enabled(module_id)) {
        return;
    }

    /* Check if log level is enabled */
    if (!ww_log_is_level_enabled(level)) {
        return;
    }

#ifdef CONFIG_WW_LOG_OUTPUT_UART
    /* Extract filename without path */
    const char *basename = ww_log_extract_filename(filename);

    /* Print log header: [LEVEL][MODULE] filename:line - */
    printf("[%s][%s] %s:%d - ",
           ww_log_get_level_str(level),
           ww_log_get_module_str(module_id),
           basename,
           line);

    /* Print formatted message */
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    /* Print newline */
    printf("\n");
#endif
}

#endif /* CONFIG_WW_LOG_STR_MODE */
