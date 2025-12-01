/**
 * @file brom_boot.c
 * @brief Boot ROM module
 * @date 2025-11-29
 *
 * Example: File with custom offset for precise differentiation
 */

/* Define file offset BEFORE including module header */
#define CURRENT_FILE_OFFSET  BROM_FILE_BOOT

#include "brom_in.h"

/**
 * @brief Execute boot sequence
 */
void brom_boot_execute(void)
{
    /* Encode mode: LOG_ID=161 (160+1) */
    LOG_INF(CURRENT_MODULE_TAG, "Starting boot sequence...");

    int boot_stage = 1;

    LOG_DBG(CURRENT_MODULE_TAG, "Boot stage 1: Hardware initialization");

    boot_stage = 2;
    LOG_DBG(CURRENT_MODULE_TAG, "Boot stage 2: Memory test");

    int memory_ok = 1;
    if (!memory_ok) {
        LOG_ERR(CURRENT_MODULE_TAG, "Memory test failed!");
        return;
    }

    boot_stage = 3;
#ifdef WW_LOG_MODE_ENCODE
    (void)boot_stage;  /* Suppress unused warning in encode mode */
#endif
    LOG_INF(CURRENT_MODULE_TAG, "Boot stage 3 completed, stage=%d", boot_stage);

    LOG_INF(CURRENT_MODULE_TAG, "Boot sequence completed successfully");
}

/**
 * @brief Check boot status
 */
void brom_boot_check(void)
{
    int boot_count = 5;
    int last_error = 0;
#ifdef WW_LOG_MODE_ENCODE
    (void)boot_count;   /* Suppress unused warning in encode mode */
#endif

    LOG_DBG(CURRENT_MODULE_TAG, "Checking boot status...");

    if (last_error != 0) {
        LOG_WRN(CURRENT_MODULE_TAG, "Previous boot had errors, count=%d, error=%d",
                         boot_count, last_error);
    } else {
        LOG_INF(CURRENT_MODULE_TAG, "Boot status OK, count=%d", boot_count);
    }
}
