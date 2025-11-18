/**
 * @file log_file_id.h
 * @brief File ID enumeration for log system
 * @date 2025-11-18
 *
 * Each source file is assigned a unique 12-bit ID (0-4095)
 * IDs are organized by module with reserved ranges for future expansion
 */

#ifndef LOG_FILE_ID_H
#define LOG_FILE_ID_H

/**
 * File ID enumeration
 *
 * Organization:
 * - DEMO Module:    1-50
 * - TEST Module:    51-100
 * - APP Module:     101-150
 * - DRIVERS Module: 151-200
 * - BROM Module:    201-250
 * - Reserved:       251-4095
 */
typedef enum {
    /* DEMO Module (1-50) */
    FILE_ID_DEMO_INIT = 1,          /* src/demo/demo_init.c */
    FILE_ID_DEMO_PROCESS = 2,       /* src/demo/demo_process.c */
    /* 3-50: Reserved for future DEMO files */

    /* TEST Module (51-100) */
    FILE_ID_TEST_UNIT = 51,         /* src/test/test_unit.c */
    FILE_ID_TEST_INTEGRATION = 52,  /* src/test/test_integration.c */
    FILE_ID_TEST_STRESS = 53,       /* src/test/test_stress.c */
    /* 54-100: Reserved for future TEST files */

    /* APP Module (101-150) */
    FILE_ID_APP_MAIN = 101,         /* src/app/app_main.c */
    FILE_ID_APP_CONFIG = 102,       /* src/app/app_config.c */
    /* 103-150: Reserved for future APP files */

    /* DRIVERS Module (151-200) */
    FILE_ID_DRV_UART = 151,         /* src/drivers/drv_uart.c */
    FILE_ID_DRV_SPI = 152,          /* src/drivers/drv_spi.c */
    FILE_ID_DRV_I2C = 153,          /* src/drivers/drv_i2c.c */
    /* 154-200: Reserved for future DRIVERS files */

    /* BROM Module (201-250) */
    FILE_ID_BROM_BOOT = 201,        /* src/brom/brom_boot.c */
    FILE_ID_BROM_LOADER = 202,      /* src/brom/brom_loader.c */
    /* 203-250: Reserved for future BROM files */

    /* Reserved for future modules (251-4095) */

} LOG_FILE_ID_E;

#endif /* LOG_FILE_ID_H */
