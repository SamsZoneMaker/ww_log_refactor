/**
 * @file brom_boot.c
 * @brief Boot ROM module
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_BROM_BOOT
#define CURRENT_MODULE_ID WW_LOG_MOD_BROM

/**
 * @brief Execute boot sequence
 */
void brom_boot_execute(void)
{
    TEST_LOG_INF_MSG("Starting boot sequence...");

    int boot_stage = 1;

    TEST_LOG_DBG_MSG("Boot stage 1: Hardware initialization");

    boot_stage = 2;
    TEST_LOG_DBG_MSG("Boot stage 2: Memory test");

    int memory_ok = 1;
    if (!memory_ok) {
        TEST_LOG_ERR_MSG("Memory test failed!");
        return;
    }

    boot_stage = 3;
    TEST_LOG_INF_MSG("Boot stage 3 completed, stage=%d", boot_stage);

    TEST_LOG_INF_MSG("Boot sequence completed successfully");
}

/**
 * @brief Check boot status
 */
void brom_boot_check(void)
{
    int boot_count = 5;
    int last_error = 0;

    TEST_LOG_DBG_MSG("Checking boot status...");

    if (last_error != 0) {
        TEST_LOG_WRN_MSG("Previous boot had errors, count=%d, error=%d",
                         boot_count, last_error);
    } else {
        TEST_LOG_INF_MSG("Boot status OK, count=%d", boot_count);
    }
}
