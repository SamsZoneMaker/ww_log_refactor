/**
 * @file demo_in.h
 * @brief DEMO module internal header
 * @date 2025-11-29
 *
 * This header provides DEMO module-specific definitions including:
 * - Module base ID configuration
 * - File offset enumeration (for fine-grained file differentiation)
 * - Current log ID calculation
 *
 * Usage:
 *   // Normal file (uses default offset 0)
 *   #include "demo_in.h"
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
#define CURRENT_MODULE_BASE   WW_LOG_MOD_DEMO_BASE

/**
 * Module name tag for string mode output
 */
#define CURRENT_MODULE_TAG    "[DEMO]"

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

/* ========== Module API (if needed) ========== */

/* Add module-specific function declarations here */
void demo_init(void);
void demo_process(int task_id);

#endif /* DEMO_IN_H */
