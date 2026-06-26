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

/* ======================================================================
 * Utility return-code macros
 * These match the naming used in n_ww_logoutput.c but are NOT defined in
 * n_ww_log_control.h.  Bug: the firmware must define them in its own
 * ww_std.h; we provide them here for the sim.
 * ====================================================================== */

/** Return from a void function if expr is true; rc is unused (compat signature). */
#define N_RETURN_IF_TRUE_WO_PRINT(expr, rc) \
    do { if (expr) { (void)(rc); return; } } while (0)

/** Return rc from a non-void function if expr is true; no log print. */
#define N_RETURN_CODE_IF_TRUE_WO_PRINT(expr, rc) \
    do { if (expr) { return (rc); } } while (0)

#endif /* __WW_STD_H__ */
