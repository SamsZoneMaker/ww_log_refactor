/**
 * @file ww_log_str.c
 * @brief String mode logging implementation
 * @date 2025-11-29
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
 * @brief Core string mode output function
 * @param module Module tag string (e.g., "[DEMO]", "[BROM]")
 * @param level Log level (0-3)
 * @param filename Source filename (without path)
 * @param line Line number
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 *
 * Output format: [MODULE][LEVEL][filename:line] message
 * Example: [BROM][INF][brom_boot.c:42] Boot sequence started
 */
void ww_log_str_output(const char *module, U8 level,
                       const char *filename, U32 line,
                       const char *fmt, ...)
{
    va_list args;

    /* Validate level */
    if (level > WW_LOG_LEVEL_DBG) {
        level = WW_LOG_LEVEL_DBG;
    }

    /* Print header: [MODULE][LEVEL][filename:line] */
    printf("%s[%s][%s:%u] ",
           module,
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
