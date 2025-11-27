/**
 * @file ww_log_str.c
 * @brief String mode implementation
 * @date 2025-11-18
 */

#include "ww_log.h"
#include <stdarg.h>
#include <string.h>

#ifdef CONFIG_WW_LOG_STR_MODE

/* ========== Output Hook ========== */

/**
 * Custom output hook (NULL = use default printf)
 */
static ww_log_str_hook_t g_str_output_hook = NULL;

/**
 * @brief Install custom output hook for string mode
 */
void ww_log_str_hook_install(ww_log_str_hook_t fn)
{
    g_str_output_hook = fn;
}

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
    char buffer[256];
    int offset = 0;

    /* Check if module is enabled */
    if (!ww_log_is_mod_enabled(module_id)) {
        return;
    }

    /* Check if log level is enabled */
    if (!ww_log_is_level_enabled(level)) {
        return;
    }

    /* Extract filename without path */
    const char *basename = ww_log_extract_filename(filename);

    /* Format log header: [LEVEL][MODULE] filename:line - */
    offset = snprintf(buffer, sizeof(buffer), "[%s][%s] %s:%d - ",
                      ww_log_get_level_str(level),
                      ww_log_get_module_str(module_id),
                      basename,
                      line);

    /* Format message */
    if (offset > 0 && offset < (int)sizeof(buffer)) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
        va_end(args);
    }

    /* Output to UART (via hook or printf) */
#ifdef CONFIG_WW_LOG_OUTPUT_UART
    if (g_str_output_hook) {
        g_str_output_hook(buffer);
    } else {
        printf("%s\n", buffer);
    }
#endif

    /* Output to RAM buffer (if enabled) */
#ifdef CONFIG_WW_LOG_OUTPUT_RAM
    /* TODO: Implement string-to-RAM storage if needed */
    /* For now, RAM buffer is mainly used in ENCODE mode */
#endif
}

#endif /* CONFIG_WW_LOG_STR_MODE */
