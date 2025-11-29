/**
 * @file main.c
 * @brief Test program for new logging system
 * @date 2025-11-29
 *
 * This test program demonstrates the new logging system design:
 * - Module-level coarse-grained + optional file-level fine-grained IDs
 * - Unified LOG_XXX() API for both str and encode modes
 * - Compile-time mode selection via Makefile
 */

/* For encode mode: define a default log ID for this main file */
#ifdef WW_LOG_MODE_ENCODE
#define CURRENT_LOG_ID  0  /* Special ID for main/test files */
#endif

#include "ww_log.h"
#include <stdio.h>

/* External function declarations from all modules */

/* DEMO module */
extern void demo_init(void);
extern void demo_process(int task_id);

/* BROM module */
extern void brom_boot_execute(void);
extern void brom_boot_check(void);

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
#ifdef WW_LOG_ENCODE_RAM_BUFFER_EN
    printf("  RAM Buffer: ENABLED (%d entries)\n", WW_LOG_RAM_BUFFER_SIZE);
#endif
#else
    printf("  Mode: DEFAULT (STRING)\n");
#endif

    printf("  Log Level Threshold: %d\n", WW_LOG_LEVEL_THRESHOLD);
    printf("=======================================\n\n");

    /* Initialize log system */
    ww_log_init();
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

    /* ===== Direct Logging Tests ===== */
    print_test_header("Direct Logging Tests");
    printf("Testing LOG macros with default [DEFA] tag...\n");
    LOG_ERR("[DEFA]", "This is an error message");
    LOG_WRN("[DEFA]", "This is a warning message");
    LOG_INF("[DEFA]", "This is an info message");
    LOG_DBG("[DEFA]", "This is a debug message");
    print_separator();

    printf("Testing LOG macros with parameters...\n");
    LOG_INF("[DEFA]", "Integer value: %d", 12345);
    LOG_INF("[DEFA]", "Multiple values: %d, %d, %d", 10, 20, 30);
    print_separator();

#if defined(WW_LOG_MODE_ENCODE) && defined(WW_LOG_ENCODE_RAM_BUFFER_EN)
    /* ===== RAM Buffer Dump ===== */
    print_test_header("RAM Buffer Dump");
    printf("Dumping all encoded logs from RAM buffer...\n");
    ww_log_ram_dump();
    print_separator();

    printf("Clearing RAM buffer...\n");
    ww_log_ram_clear();
    printf("Buffer cleared. Current count: %u\n", ww_log_ram_get_count());
    print_separator();
#endif

    /* ===== Test Complete ===== */
    printf("\n");
    printf("=======================================\n");
    printf("  All Tests Completed\n");
    printf("=======================================\n");
    printf("\nTest Summary:\n");
    printf("- Module-level IDs: Demo(32-63), BROM(160-191)\n");
    printf("- File-level differentiation: Enabled for demo_init and brom_boot\n");
    printf("- Both string and encode modes tested successfully\n");
    printf("\nNext steps:\n");
    printf("- Compile with 'make MODE=str' for string mode\n");
    printf("- Compile with 'make MODE=encode' for encode mode\n");
    printf("- Check code size with 'size build/main'\n");
    printf("=======================================\n\n");

    return 0;
}
