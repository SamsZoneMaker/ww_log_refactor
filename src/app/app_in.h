/**
 * @file app_in.h
 * @brief APP module internal header
 * @date 2025-11-29
 */

#ifndef APP_IN_H
#define APP_IN_H

#include "file_id.h"
#include "ww_log.h"

/* ========== Module Configuration ========== */

/**
 * Module base ID
 * APP module occupies LOG_ID range: 96-127
 */
#define CURRENT_MODULE_BASE   WW_LOG_MOD_APP_BASE

/**
 * Module name tag for string mode output
 */
#define CURRENT_MODULE_TAG    "[APP]"

/* ========== File Offset Configuration ========== */

/**
 * File offset enumeration for APP module files
 */
typedef enum {
    APP_FILE_DEFAULT   = 0,   /* Default offset */
    APP_FILE_MAIN      = 1,   /* app_main.c -> LOG_ID = 97 */
    APP_FILE_CONFIG    = 2,   /* app_config.c -> LOG_ID = 98 */
} APP_FILE_OFFSET_E;

/**
 * Current file offset (default to 0 if not defined)
 */
#ifndef CURRENT_FILE_OFFSET
#define CURRENT_FILE_OFFSET   APP_FILE_DEFAULT
#endif

/**
 * Current log ID = MODULE_BASE + FILE_OFFSET
 */
#define CURRENT_LOG_ID        (CURRENT_MODULE_BASE + CURRENT_FILE_OFFSET)

#endif /* APP_IN_H */
