/**
 * @file demo_in.h
 * @brief DEMO module internal header
 * @date 2025-11-29
 *
 * This header provides DEMO module-specific definitions including:
 * - Module base ID configuration
 * - File offset enumeration (for fine-grained file differentiation)
 * - Current log ID calculation
 * - Static switch for compile-time log removal
 *
 * Static Switch Usage:
 *   Define WW_LOG_MODULE_DEMO_EN in your build system to enable DEMO logs.
 *   If not defined, all DEMO_LOG_xxx macros compile to nothing.
 *
 * Usage:
 *   // Normal file (uses default offset 0)
 *   #include "demo_in.h"
 *   DEMO_LOG_INF("Hello");  // Uses module-specific macro
 *
 *   // File with custom offset (for differentiation)
 *   #define CURRENT_FILE_OFFSET  DEMO_FILE_INIT
 *   #include "demo_in.h"
 */

#ifndef DEMO_IN_H
#define DEMO_IN_H

#include "file_id.h"
#include "ww_log.h"

/* ========== Module Configuration ========== */

/**
 * Module base ID
 * DEMO module occupies LOG_ID range: 32-63
 */
#define CURRENT_MODULE_BASE   WW_LOG_DIR_DEMO_BASE

/**
 * Module name tag (for encode mode, as string)
 */
#define CURRENT_MODULE_TAG    "[DEMO]"

/**
 * Module ID (for string mode and module filtering)
 * Note: str mode requires module_id, encode mode uses module_tag string
 * We define CURRENT_LOG_PARAM to be compatible with both modes
 */
#define CURRENT_MODULE_ID     WW_LOG_MODULE_DEMO

#ifdef WW_LOG_MODE_STR
#define CURRENT_LOG_PARAM     CURRENT_MODULE_ID
#else
#define CURRENT_LOG_PARAM     CURRENT_MODULE_TAG
#endif

/* ========== File Offset Configuration ========== */

/**
 * File offset enumeration
 * Allows differentiating between specific files within this module
 *
 * Default: All files use offset 0 (FILE_ID = 32)
 * Custom: Define CURRENT_FILE_OFFSET before including this header
 */
typedef enum {
    DEMO_FILE_DEFAULT   = 0,   /* Default offset, for most files */
    DEMO_FILE_INIT      = 1,   /* demo_init.c -> LOG_ID = 32 + 1 = 33 */
    DEMO_FILE_PROCESS   = 2,   /* demo_process.c -> LOG_ID = 32 + 2 = 34 */
    /* Add more files as needed (up to offset 31) */
} DEMO_FILE_OFFSET_E;

/**
 * Current file offset
 * Default to 0 if not defined by the source file
 */
#ifndef CURRENT_FILE_OFFSET
#define CURRENT_FILE_OFFSET   DEMO_FILE_DEFAULT
#endif

/**
 * Current log ID
 * Calculated as: MODULE_BASE + FILE_OFFSET
 */
#define CURRENT_LOG_ID        (CURRENT_MODULE_BASE + CURRENT_FILE_OFFSET)

/* ========== Module Static Switch ========== */

/**
 * Static switch for DEMO module logs
 *
 * When WW_LOG_MODULE_DEMO_EN is defined:
 *   - DEMO_LOG_xxx macros expand to normal LOG_xxx calls
 *   - Log code is compiled into the binary
 *
 * When WW_LOG_MODULE_DEMO_EN is NOT defined:
 *   - DEMO_LOG_xxx macros expand to nothing (do{}while(0))
 *   - No log code is compiled - zero code size overhead
 *
 * Define WW_LOG_MODULE_DEMO_EN in:
 *   - Makefile: -DWW_LOG_MODULE_DEMO_EN
 *   - Or in a project-wide config header
 */
#ifdef WW_LOG_MODULE_DEMO_EN

    /* DEMO logs enabled - use normal LOG macros with module ID */
    #define DEMO_LOG_ERR(fmt, ...)  LOG_ERR(CURRENT_MODULE_ID, fmt, ##__VA_ARGS__)
    #define DEMO_LOG_WRN(fmt, ...)  LOG_WRN(CURRENT_MODULE_ID, fmt, ##__VA_ARGS__)
    #define DEMO_LOG_INF(fmt, ...)  LOG_INF(CURRENT_MODULE_ID, fmt, ##__VA_ARGS__)
    #define DEMO_LOG_DBG(fmt, ...)  LOG_DBG(CURRENT_MODULE_ID, fmt, ##__VA_ARGS__)

#else

    /* DEMO logs disabled - compile to nothing */
    #define DEMO_LOG_ERR(fmt, ...)  do { } while(0)
    #define DEMO_LOG_WRN(fmt, ...)  do { } while(0)
    #define DEMO_LOG_INF(fmt, ...)  do { } while(0)
    #define DEMO_LOG_DBG(fmt, ...)  do { } while(0)

#endif /* WW_LOG_MODULE_DEMO_EN */

/* ========== Module API (if needed) ========== */

/* Add module-specific function declarations here */
void demo_init(void);
void demo_process(int task_id);

#endif /* DEMO_IN_H */
