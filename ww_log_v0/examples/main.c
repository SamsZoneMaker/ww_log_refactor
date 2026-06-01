/**
 * @file main.c
 * @brief Test program for new logging system (Auto File ID)
 * @date 2025-12-17
 *
 * This test program demonstrates the new logging system design:
 * - Automatic file ID injection via Makefile
 * - Unified LOG_XXX() API for both str and encode modes
 * - Compile-time mode selection via Makefile
 * - No manual CURRENT_FILE_ID definition needed!
 * - Phase2: RAM buffer and flush mechanism
 */

#include "ww_log.h"
#include <stdio.h>

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
#include "ww_log_flush.h"
#include "ww_log_ram.h"
#endif

/* External function declarations from all modules */

/* DEMO module */
extern void demo_init(void);
extern void demo_process(int task_id);

/* BROM module */
extern void brom_boot_execute(void);
extern void brom_boot_check(void);

/* TEST module */
extern void test_unit_run(void);
extern void test_integration_run(void);
extern void test_stress_run(void);

/* APP module */
extern void app_main(void);
extern void app_config_load(void);
extern void app_config_save(void);

/* DRIVERS module */
extern void drv_uart_init(void);
extern void drv_uart_send(int length);
extern void drv_spi_init(void);
extern void drv_spi_transfer(int tx_len, int rx_len);
extern void drv_i2c_init(void);
extern void drv_i2c_read(int device_addr, int reg_addr);
extern void drv_i2c_write(int device_addr, int data);

/**
 * @brief Print test header
 */
static void print_test_header(const char *title)
{
    printf("\n");
    printf("========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

/**
 * @brief Print section separator
 */
static void print_separator(void)
{
    printf("\n");
}

/**
 * @brief Main function
 */
int main(void)
{
    printf("\n");
    printf("=======================================\n");
    printf("  Log System Test Program (New Design)\n");
    printf("=======================================\n");

    /* Show current mode */
#if defined(WW_LOG_MODE_DISABLED)
    printf("  Mode: DISABLED\n");
#elif defined(WW_LOG_MODE_STR)
    printf("  Mode: STRING MODE\n");
#elif defined(WW_LOG_MODE_ENCODE)
    printf("  Mode: ENCODE MODE\n");
#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    printf("  RAM Buffer: ENABLED (4KB)\n");
    printf("  Flush Threshold: 3KB\n");
#endif
#else
    printf("  Mode: DEFAULT (STRING)\n");
#endif

    printf("  Log Level Threshold: %d (runtime configurable)\n", ww_log_get_level_threshold());
    printf("=======================================\n\n");

    /* Initialize log system */
    ww_log_init();

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    /* Initialize flush mechanism */
    log_flush_init();
    printf("LOG flush mechanism initialized\n");
#endif
    print_separator();

    /* ===== DEMO Module Tests ===== */
    print_test_header("DEMO Module Tests");
    printf("Testing demo_init() with custom file offset (LOG_ID=33)...\n");
    demo_init();
    print_separator();

    printf("Testing demo_process() with custom file offset (LOG_ID=34)...\n");
    demo_process(42);
    print_separator();

    /* ===== BROM Module Tests ===== */
    print_test_header("BROM Module Tests");
    printf("Testing brom_boot_execute() with custom file offset (LOG_ID=161)...\n");
    brom_boot_execute();
    print_separator();

    printf("Testing brom_boot_check() with custom file offset (LOG_ID=161)...\n");
    brom_boot_check();
    print_separator();

    /* ===== TEST Module Tests ===== */
    print_test_header("TEST Module Tests");
    printf("Testing test_unit_run() with custom file offset (LOG_ID=65)...\n");
    test_unit_run();
    print_separator();

    printf("Testing test_integration_run() with custom file offset (LOG_ID=66)...\n");
    test_integration_run();
    print_separator();

    printf("Testing test_stress_run() with custom file offset (LOG_ID=67)...\n");
    test_stress_run();
    print_separator();

    /* ===== APP Module Tests ===== */
    print_test_header("APP Module Tests");
    printf("Testing app_main() with custom file offset (LOG_ID=97)...\n");
    app_main();
    print_separator();

    printf("Testing app_config_load() with custom file offset (LOG_ID=98)...\n");
    app_config_load();
    print_separator();

    printf("Testing app_config_save() with custom file offset (LOG_ID=98)...\n");
    app_config_save();
    print_separator();

    /* ===== DRIVERS Module Tests ===== */
    print_test_header("DRIVERS Module Tests");
    printf("Testing drv_uart_init() with custom file offset (LOG_ID=129)...\n");
    drv_uart_init();
    print_separator();

    printf("Testing drv_uart_send()...\n");
    drv_uart_send(128);
    print_separator();

    printf("Testing drv_spi_init() with custom file offset (LOG_ID=130)...\n");
    drv_spi_init();
    print_separator();

    printf("Testing drv_spi_transfer()...\n");
    drv_spi_transfer(64, 64);
    print_separator();

    printf("Testing drv_i2c_init() with custom file offset (LOG_ID=131)...\n");
    drv_i2c_init();
    print_separator();

    printf("Testing drv_i2c_read()...\n");
    drv_i2c_read(0x50, 0x10);
    print_separator();

    printf("Testing drv_i2c_write()...\n");
    drv_i2c_write(0x50, 0xAB);
    print_separator();

    /* ===== Direct Logging Tests ===== */
    print_test_header("Direct Logging Tests");
    printf("Testing LOG macros with DEFAULT module (module_id=0)...\n");
    LOG_ERR("This is an error message");
    LOG_WRN("This is a warning message");
    LOG_INF("This is an info message");
    LOG_DBG("This is a debug message");
    print_separator();

    printf("Testing LOG macros with parameters (DEFAULT module)...\n");
    LOG_INF("Integer value: %d", 12345);
    LOG_INF("Multiple values: %d, %d, %d", 10, 20, 30);
    print_separator();

    printf("Testing LOG macros with different module IDs...\n");
    LOG_ERR("TEST module error message");
    LOG_INF("APP module log: value=%d", 999);
    print_separator();

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    /* ===== Flush Mechanism Test ===== */
    print_test_header("Flush Mechanism Test");

    /* Process any pending flush requests */
    printf("Processing flush requests...\n");
    log_flush_process();

    /* Show RAM buffer status */
    printf("RAM buffer usage: %u bytes\n", log_ram_get_usage());
    printf("RAM buffer available: %u bytes\n", log_ram_get_available());

    /* Get flush statistics */
    U32 total_flushes, failed_flushes;
    U16 last_flush_size;
    log_flush_get_stats(&total_flushes, &failed_flushes, &last_flush_size);
    printf("Flush statistics:\n");
    printf("  Total flushes: %u\n", total_flushes);
    printf("  Failed flushes: %u\n", failed_flushes);
    printf("  Last flush size: %u bytes\n", last_flush_size);
    print_separator();

    /* ===== RAM Buffer Dump ===== */
    print_test_header("RAM Buffer Dump");
    printf("Dumping RAM buffer content...\n");
    log_ram_dump_hex();
    print_separator();

    /* Manual flush test */
    print_test_header("Manual Flush Test");
    printf("Triggering manual flush...\n");
    int flush_ret = log_flush_now();
    if (flush_ret == 0) {
        printf("Manual flush completed successfully\n");
    } else {
        printf("Manual flush failed with error: %d\n", flush_ret);
    }
    printf("RAM buffer usage after flush: %u bytes\n", log_ram_get_usage());
    print_separator();
#endif

    /* ===== Test Complete ===== */
    printf("\n");
    printf("=======================================\n");
    printf("  All Tests Completed\n");
    printf("=======================================\n");
    printf("\nTest Summary:\n");
    printf("- All 5 modules tested: DEMO, BROM, TEST, APP, DRIVERS\n");
    printf("- Module-level IDs (64 files per module):\n");
    printf("  DEFAULT(0-63), DEMO(64-127), TEST(128-191)\n");
    printf("  APP(192-255), DRV(256-319), BROM(320-383)\n");
    printf("- File-level differentiation: Enabled in all modules\n");
    printf("- Optional module parameter: Defaults to [DEFA] when not specified\n");
    printf("- Both string and encode modes supported\n");
#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    printf("- Phase2: RAM buffer and flush mechanism tested\n");
#endif
    printf("\nNext steps:\n");
    printf("- Compile with 'make MODE=str' for string mode\n");
    printf("- Compile with 'make MODE=encode' for encode mode\n");
    printf("- Check code size with 'size bin/log_test_{str,encode}'\n");
    printf("- Decode binary logs with 'tools/log_decoder.py'\n");
#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    printf("\nIMPORTANT for Phase2:\n");
    printf("- In your application, call log_flush_process() periodically\n");
    printf("- Recommended: Call it in main loop every 10-100ms\n");
    printf("- Or use log_flush_now() for immediate flush before critical events\n");
#endif
    printf("=======================================\n\n");

    return 0;
}
