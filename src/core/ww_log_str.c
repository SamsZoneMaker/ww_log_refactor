/**
 * @file ww_log_str.c
 * @brief String mode logging implementation (refactored)
 * @date 2025-12-01
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
 * @brief Core string mode output function (refactored)
 * @param filename Source filename (without path)
 * @param line Line number
 * @param level Log level (0-3)
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 *
 * Output format: [LEVEL] filename:line - message
 * Example: [INF] brom_boot.c:42 - Boot sequence started
 */
void ww_log_str_output(const char *filename, U32 line, U8 level,
                       const char *fmt, ...)
{
    va_list args;

    /* Validate level */
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
