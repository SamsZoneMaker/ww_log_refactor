/**
 * @file brom_boot.c
 * @brief BROM boot sequence
 * @date 2025-11-29
 */

#include "brom_in.h"

/**
 * @brief Execute boot sequence
 */
void brom_boot_execute(void)
{
    /* File ID is automatically injected by Makefile via -DCURRENT_FILE_ID=xxx */
    LOG_INF("Starting boot sequence...");

    LOG_DBG("Boot stage 1: Hardware initialization");

    LOG_DBG("Boot stage 2: Memory test");

    int memory_ok = 1;
    if (!memory_ok) {
        LOG_ERR("Memory test failed!");
        return;
    }

#ifdef ENABLE_FLASH_CHECK
    LOG_DBG("Boot stage 3: Flash verification");
#endif
    LOG_INF("Boot stage 3 completed, stage=%d", 333);

    LOG_INF("Boot sequence completed successfully");
}

/**
 * @brief Check boot status
 */
void brom_boot_check(void)
{
    int boot_count = 42;
    int last_error = 0;

    LOG_DBG("Checking boot status...");

    if (last_error != 0) {
        LOG_WRN("Previous boot had errors, count=%d, error=%d", boot_count, last_error);
    } else {
        LOG_INF("Boot status OK, count=%d", boot_count);
    }
}
