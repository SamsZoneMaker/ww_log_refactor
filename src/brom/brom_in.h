/**
 * @file brom_in.h
 * @brief BROM module internal header
 * @date 2025-11-29
 */

#ifndef BROM_IN_H
#define BROM_IN_H

#include "file_id.h"
#include "ww_log.h"

/* ========== Module Configuration ========== */

/**
 * Module base ID
 * BROM module occupies LOG_ID range: 160-191
 */
#define CURRENT_MODULE_BASE   WW_LOG_DIR_BROM_BASE

/**
 * Module name tag for string mode output
 */
#define CURRENT_MODULE_TAG    "[BROM]"

#define CURRENT_MODULE_ID     WW_LOG_MODULE_BROM

#ifdef WW_LOG_MODE_STR
#define CURRENT_LOG_PARAM     CURRENT_MODULE_ID
#else
#define CURRENT_LOG_PARAM     CURRENT_MODULE_TAG
#endif

/* ========== File Offset Configuration ========== */

/**
 * File offset enumeration for BROM module files
 */
typedef enum {
    BROM_FILE_DEFAULT   = 0,   /* Default offset */
    BROM_FILE_BOOT      = 1,   /* brom_boot.c -> LOG_ID = 161 */
    BROM_FILE_LOADER    = 2,   /* brom_loader.c -> LOG_ID = 162 */
    BROM_FILE_FLASH     = 3,   /* brom_flash.c -> LOG_ID = 163 */
    BROM_FILE_EEPROM    = 4,   /* brom_eeprom.c -> LOG_ID = 164 */
} BROM_FILE_OFFSET_E;

/**
 * Current file offset (default to 0 if not defined)
 */
#ifndef CURRENT_FILE_OFFSET
#define CURRENT_FILE_OFFSET   BROM_FILE_DEFAULT
#endif

/**
 * Current log ID = MODULE_BASE + FILE_OFFSET
 */
#define CURRENT_LOG_ID        (CURRENT_MODULE_BASE + CURRENT_FILE_OFFSET)

#endif /* BROM_IN_H */
