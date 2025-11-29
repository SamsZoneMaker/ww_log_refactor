/**
 * @file file_id.h
 * @brief Module base ID definitions for log system
 * @date 2025-11-29
 *
 * Design:
 * - Each module has a base ID with 32 ID slots reserved
 * - LOG_ID = MODULE_BASE + FILE_OFFSET (0-31)
 * - MODULE_BASE is computed as (N << 5) to reserve 32 slots
 * - 12-bit LOG_ID space supports up to 128 modules
 *
 * ID Allocation:
 * - DEMO Module:    32-63   (base: 1 << 5 = 32)
 * - TEST Module:    64-95   (base: 2 << 5 = 64)
 * - APP Module:     96-127  (base: 3 << 5 = 96)
 * - DRIVERS Module: 128-159 (base: 4 << 5 = 128)
 * - BROM Module:    160-191 (base: 5 << 5 = 160)
 * - Reserved:       192-4095 (for future modules)
 */

#ifndef FILE_ID_H
#define FILE_ID_H

/**
 * Module base ID enumeration
 * Each module reserves 32 ID slots (0-31 offset within module)
 */
typedef enum {
    WW_LOG_MOD_DEMO_BASE  = (1 << 5),   /* 32-63,   DEMO module */
    WW_LOG_MOD_TEST_BASE  = (2 << 5),   /* 64-95,   TEST module */
    WW_LOG_MOD_APP_BASE   = (3 << 5),   /* 96-127,  APP module */
    WW_LOG_MOD_DRV_BASE   = (4 << 5),   /* 128-159, DRIVERS module */
    WW_LOG_MOD_BROM_BASE  = (5 << 5),   /* 160-191, BROM module */

    /* Add more modules here as needed */
    /* Maximum: (127 << 5) = 4064-4095 */
} WW_LOG_MOD_BASE_E;

/**
 * Optional: Pre-defined file IDs for important files
 * These are the full LOG_ID values (MODULE_BASE + FILE_OFFSET)
 *
 * Most files use FILE_OFFSET=0 (default), but critical files
 * can be pre-defined here for documentation purposes.
 */
typedef enum {
    /* DEMO module specific files */
    FILE_ID_DEMO_DEFAULT  = WW_LOG_MOD_DEMO_BASE + 0,  /* 32, default for all demo files */
    FILE_ID_DEMO_INIT     = WW_LOG_MOD_DEMO_BASE + 1,  /* 33, demo_init.c */
    FILE_ID_DEMO_PROCESS  = WW_LOG_MOD_DEMO_BASE + 2,  /* 34, demo_process.c */
    /* 35-63: Reserved for DEMO */

    /* TEST module specific files */
    FILE_ID_TEST_DEFAULT      = WW_LOG_MOD_TEST_BASE + 0,  /* 64, default */
    FILE_ID_TEST_UNIT         = WW_LOG_MOD_TEST_BASE + 1,  /* 65, test_unit.c */
    FILE_ID_TEST_INTEGRATION  = WW_LOG_MOD_TEST_BASE + 2,  /* 66, test_integration.c */
    FILE_ID_TEST_STRESS       = WW_LOG_MOD_TEST_BASE + 3,  /* 67, test_stress.c */
    /* 68-95: Reserved for TEST */

    /* APP module specific files */
    FILE_ID_APP_DEFAULT   = WW_LOG_MOD_APP_BASE + 0,  /* 96, default */
    FILE_ID_APP_MAIN      = WW_LOG_MOD_APP_BASE + 1,  /* 97, app_main.c */
    FILE_ID_APP_CONFIG    = WW_LOG_MOD_APP_BASE + 2,  /* 98, app_config.c */
    /* 99-127: Reserved for APP */

    /* DRIVERS module specific files */
    FILE_ID_DRV_DEFAULT   = WW_LOG_MOD_DRV_BASE + 0,  /* 128, default */
    FILE_ID_DRV_UART      = WW_LOG_MOD_DRV_BASE + 1,  /* 129, drv_uart.c */
    FILE_ID_DRV_SPI       = WW_LOG_MOD_DRV_BASE + 2,  /* 130, drv_spi.c */
    FILE_ID_DRV_I2C       = WW_LOG_MOD_DRV_BASE + 3,  /* 131, drv_i2c.c */
    /* 132-159: Reserved for DRIVERS */

    /* BROM module specific files */
    FILE_ID_BROM_DEFAULT  = WW_LOG_MOD_BROM_BASE + 0,  /* 160, default */
    FILE_ID_BROM_BOOT     = WW_LOG_MOD_BROM_BASE + 1,  /* 161, brom_boot.c */
    FILE_ID_BROM_LOADER   = WW_LOG_MOD_BROM_BASE + 2,  /* 162, brom_loader.c */
    FILE_ID_BROM_FLASH    = WW_LOG_MOD_BROM_BASE + 3,  /* 163, brom_flash.c */
    FILE_ID_BROM_EEPROM   = WW_LOG_MOD_BROM_BASE + 4,  /* 164, brom_eeprom.c */
    /* 165-191: Reserved for BROM */

} WW_LOG_FILE_ID_E;

#endif /* FILE_ID_H */
