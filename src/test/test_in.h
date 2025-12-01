/**
 * @file test_in.h
 * @brief TEST module internal header
 * @date 2025-11-29
 */

#ifndef TEST_IN_H
#define TEST_IN_H

#include "file_id.h"
#include "ww_log.h"

/* ========== Module Configuration ========== */

/**
 * Module base ID
 * TEST module occupies LOG_ID range: 64-95
 */
#define CURRENT_MODULE_BASE   WW_LOG_DIR_TEST_BASE

/**
 * Module name tag for string mode output
 */
#define CURRENT_MODULE_TAG    "[TEST]"

/**
 * Module ID (for string mode and module filtering)
 * Note: str mode requires module_id, encode mode uses module_tag string
 * We define CURRENT_LOG_PARAM to be compatible with both modes
 */
#define CURRENT_MODULE_ID     WW_LOG_MODULE_TEST

#ifdef WW_LOG_MODE_STR
#define CURRENT_LOG_PARAM     CURRENT_MODULE_ID
#else
#define CURRENT_LOG_PARAM     CURRENT_MODULE_TAG
#endif

/* ========== File Offset Configuration ========== */

/**
 * File offset enumeration for TEST module files
 */
typedef enum {
    TEST_FILE_DEFAULT      = 0,   /* Default offset */
    TEST_FILE_UNIT         = 1,   /* test_unit.c -> LOG_ID = 65 */
    TEST_FILE_INTEGRATION  = 2,   /* test_integration.c -> LOG_ID = 66 */
    TEST_FILE_STRESS       = 3,   /* test_stress.c -> LOG_ID = 67 */
} TEST_FILE_OFFSET_E;

/**
 * Current file offset (default to 0 if not defined)
 */
#ifndef CURRENT_FILE_OFFSET
#define CURRENT_FILE_OFFSET   TEST_FILE_DEFAULT
#endif

/**
 * Current log ID = MODULE_BASE + FILE_OFFSET
 */
#define CURRENT_LOG_ID        (CURRENT_MODULE_BASE + CURRENT_FILE_OFFSET)

#endif /* TEST_IN_H */
