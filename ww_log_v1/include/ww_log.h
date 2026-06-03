/**
 * @file ww_log.h
 * @brief ww_log v1 unified entry point: mode dispatch + public API + levels
 *
 * Three compile-time modes (pick exactly ONE below):
 *   WW_LOG_MODE_STR       printf-style, human readable (debug, large)
 *   WW_LOG_MODE_ENCODE    fixed binary encoding (production, tiny)
 *   WW_LOG_MODE_DISABLED  all LOG macros vanish
 *
 * The call site is identical in every mode:
 *   LOG_ERR("msg");            LOG_WRN("x=%d", a);
 *   LOG_INF("x=%d y=%d", a, b); LOG_DBG("...");
 */

#ifndef WW_LOG_H
#define WW_LOG_H

#include "type.h"

/* ========== Log Levels ========== */

#define WW_LOG_LEVEL_ERR  0  /* Error: failures, critical issues */
#define WW_LOG_LEVEL_WRN  1  /* Warning: potential problems */
#define WW_LOG_LEVEL_INF  2  /* Info: important state changes */
#define WW_LOG_LEVEL_DBG  3  /* Debug: detailed execution flow */

/**
 * Compile-time level threshold. Logs with level > threshold are compiled
 * out entirely (zero code size). Override via -DWW_LOG_COMPILE_THRESHOLD=n.
 */
#ifndef WW_LOG_COMPILE_THRESHOLD
#define WW_LOG_COMPILE_THRESHOLD  WW_LOG_LEVEL_DBG
#endif

/* ========== Mode Selection (choose exactly one) ========== */

// #define WW_LOG_MODE_STR
#define WW_LOG_MODE_ENCODE
// #define WW_LOG_MODE_DISABLED

/* ========== Mode Dispatch ========== */

#if defined(WW_LOG_MODE_ENCODE) || defined(WW_LOG_MODE_STR)
    #include "ww_log_output.h"
#elif defined(WW_LOG_MODE_DISABLED)
    #define LOG_ERR(...)  do { } while (0)
    #define LOG_WRN(...)  do { } while (0)
    #define LOG_INF(...)  do { } while (0)
    #define LOG_DBG(...)  do { } while (0)
#else
    #error "No log mode defined! Define one of WW_LOG_MODE_STR/ENCODE/DISABLED in ww_log.h"
#endif

/* ========== Public API ========== */

/**
 * @brief Initialize the log system (backends, RAM recovery, etc.).
 */
void ww_log_init(void);

/**
 * @brief Enter panic mode: force-preserve logs on a crash (see ww_log_panic.h).
 *
 * Call from HardFault / watchdog handlers. Bypasses filtering, flushes RAM to
 * external storage synchronously, and makes subsequent logs write through.
 */
void ww_log_panic(void);

#endif /* WW_LOG_H */
