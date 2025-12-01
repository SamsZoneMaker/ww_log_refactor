/**
 * @file drv_in.h
 * @brief DRIVERS module internal header
 * @date 2025-11-29
 */

#ifndef DRV_IN_H
#define DRV_IN_H

#include "file_id.h"
#include "ww_log.h"

/* ========== Module Configuration ========== */

/**
 * Module base ID
 * DRIVERS module occupies LOG_ID range: 128-159
 */
#define CURRENT_MODULE_BASE   WW_LOG_DIR_DRV_BASE

/**
 * Module name tag for string mode output
 */
#define CURRENT_MODULE_TAG    "[DRV]"

#define CURRENT_MODULE_ID     WW_LOG_MODULE_DRIVERS

#ifdef WW_LOG_MODE_STR
#define CURRENT_LOG_PARAM     CURRENT_MODULE_ID
#else
#define CURRENT_LOG_PARAM     CURRENT_MODULE_TAG
#endif

/* ========== File Offset Configuration ========== */

/**
 * File offset enumeration for DRIVERS module files
 */
typedef enum {
    DRV_FILE_DEFAULT   = 0,   /* Default offset */
    DRV_FILE_UART      = 1,   /* drv_uart.c -> LOG_ID = 129 */
    DRV_FILE_SPI       = 2,   /* drv_spi.c -> LOG_ID = 130 */
    DRV_FILE_I2C       = 3,   /* drv_i2c.c -> LOG_ID = 131 */
} DRV_FILE_OFFSET_E;

/**
 * Current file offset (default to 0 if not defined)
 */
#ifndef CURRENT_FILE_OFFSET
#define CURRENT_FILE_OFFSET   DRV_FILE_DEFAULT
#endif

/**
 * Current log ID = MODULE_BASE + FILE_OFFSET
 */
#define CURRENT_LOG_ID        (CURRENT_MODULE_BASE + CURRENT_FILE_OFFSET)

#endif /* DRV_IN_H */
