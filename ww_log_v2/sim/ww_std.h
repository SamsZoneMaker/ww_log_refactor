/**
 * @file ww_std.h
 * @brief Sim stub: maps fw ww_std wrappers to standard C library calls.
 *
 * In the real firmware ww_std.h provides project-wide utilities.
 * For the PC sim we just redirect to <stdio.h> / <string.h>.
 */

#ifndef __WW_STD_H__
#define __WW_STD_H__

#include "def.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ======================================================================
 * Printf / formatting wrappers
 * ====================================================================== */
#define ww_printf(...)          printf(__VA_ARGS__)

static inline int ww_snprintf(char *buf, int size, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = vsnprintf(buf, (size_t)size, fmt, ap);
    va_end(ap);
    return ret;
}

/**
 * Firmware's ww_vsnprintf takes an extra "linesep" argument before fmt.
 * In the sim we ignore linesep and call standard vsnprintf.
 * TODO: VERIFY linesep semantics if output format matters on target.
 */
static inline int ww_vsnprintf(char *buf, int size, const char *linesep,
                               const char *fmt, va_list ap)
{
    (void)linesep;
    return vsnprintf(buf, (size_t)size, fmt, ap);
}

/* Line separator used in the string-mode format call */
#define LINESEP_FORMAT_WINDOWS  "\r\n"

/* ======================================================================
 * Memory wrappers
 * ====================================================================== */
#define ww_memset(dst, val, len)    memset((dst), (val), (len))
#define ww_memcpy(dst, src, len)    memcpy((dst), (src), (len))
#define ww_memcmp(a, b, len)        memcmp((a), (b), (len))

/* Note: N_RETURN_*_WO_PRINT macros are provided by n_ww_log_macro.h
 * (pulled in via n_ww_log.h below), so the sim does not redefine them here. */

/* ======================================================================
 * Logging
 * ----------------------------------------------------------------------
 * Pull the v2 log module in here so that any file that includes ww_std.h gets
 * the full N_LOG_* API with no extra include -- this is how the firmware is
 * meant to consume it. During the migration the OLD log system stays exposed
 * alongside the new one (different prefixes: legacy LOG_* vs new N_LOG_*), so
 * call sites can adopt N_LOG_* incrementally. The old log's header would be
 * included right here next to it.
 * ====================================================================== */
#include "log/n_ww_log.h"
/* #include "log/old_log.h"   <- legacy log system, kept during migration */

#endif /* __WW_STD_H__ */
