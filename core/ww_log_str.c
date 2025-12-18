/**
 * @file ww_log_str.c
 * @brief String mode logging implementation (refactored for minimal code size)
 * @date 2025-12-01
 *
 * All filtering logic (module mask, level threshold) is performed inside
 * the output function to minimize code size at each call site.
 */

#include "ww_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef WW_LOG_MODE_STR

/**
 * Level name strings for output
 */
static const char* level_names[] = {
    "ERR",  /* WW_LOG_LEVEL_ERR = 0 */
    "WRN",  /* WW_LOG_LEVEL_WRN = 1 */
    "INF",  /* WW_LOG_LEVEL_INF = 2 */
    "DBG",  /* WW_LOG_LEVEL_DBG = 3 */
};

/**
 * @brief Core string mode output function (with internal filtering)
 * @param module_id Module ID for filtering (0-31)
 * @param filename Source filename (without path)
 * @param line Line number
 * @param level Log level (0-3)
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 *
 * This function performs all checks internally:
 * 1. Module enable check (via g_ww_log_module_mask)
 * 2. Level threshold check (via g_ww_log_level_threshold)
 *
 * Output format: [LEVEL] filename:line - message
 * Example: [INF] brom_boot.c:42 - Boot sequence started
 */
void ww_log_str_output(U8 module_id, const char *filename, U32 line, U8 level,
                       const char *fmt, ...)
{
    va_list args;

    /* Check module enable (dynamic switch) */
    if ((g_ww_log_module_mask & (1U << module_id)) == 0) {
        return;
    }

    /* Check level threshold (dynamic switch) */
    if (level > g_ww_log_level_threshold) {
        return;
    }

    /* Validate level for array access */
    if (level > WW_LOG_LEVEL_DBG) {
        level = WW_LOG_LEVEL_DBG;
    }

    /* Print header: [LEVEL] filename:line - */
    printf("[%s] %s:%u - ",
           level_names[level],
           filename,
           line);

    /* Print formatted message */
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    /* Print newline */
    printf("\n");

    /* Flush output for immediate visibility */
    fflush(stdout);
}

#endif /* WW_LOG_MODE_STR */
