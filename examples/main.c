/**
 * @file main.c
 * @brief Test program for logging system
 * @date 2025-11-18
 */

#include "ww_log.h"
#include <stdio.h>

/* External function declarations from all modules */

/* DEMO module */
extern void demo_init(void);
extern void demo_process(int task_id);

/* TEST module */
extern void test_unit_run(void);
extern void test_integration_run(void);
extern void test_stress_run(void);

/* APP module */
extern void app_main(void);
extern void app_shutdown(void);
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

/* BROM module */
extern void brom_boot_execute(void);
extern void brom_boot_check(void);
extern void brom_loader_load(void);
extern void brom_loader_verify(void);
extern void brom_loader_jump(void);

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
    printf("=====================================\n");
    printf("  Log System Test Program\n");
    printf("=====================================\n");

#if defined(CONFIG_WW_LOG_DISABLED)
    printf("  Mode: DISABLED\n");
#elif defined(CONFIG_WW_LOG_STR_MODE)
    printf("  Mode: STRING MODE\n");
#elif defined(CONFIG_WW_LOG_ENCODE_MODE)
    printf("  Mode: ENCODE MODE\n");
#endif

    printf("=====================================\n\n");

    /* Initialize log system */
    ww_log_init();
    printf("Log system initialized.\n\n");

    /* ===== DEMO Module ===== */
    print_test_header("DEMO Module Tests");
    demo_init();
    print_separator();
    demo_process(42);
    print_separator();

    /* ===== TEST Module ===== */
    print_test_header("TEST Module Tests");
    test_unit_run();
    print_separator();
    test_integration_run();
    print_separator();
    test_stress_run();
    print_separator();

    /* ===== APP Module ===== */
    print_test_header("APP Module Tests");
    app_config_load();
    print_separator();
    app_main();
    print_separator();
    app_config_save();
    print_separator();
    app_shutdown();
    print_separator();

    /* ===== DRIVERS Module ===== */
    print_test_header("DRIVERS Module Tests");
    drv_uart_init();
    print_separator();
    drv_uart_send(128);
    print_separator();
    drv_spi_init();
    print_separator();
    drv_spi_transfer(16, 16);
    print_separator();
    drv_i2c_init();
    print_separator();
    drv_i2c_read(0x50, 0x00);
    print_separator();
    drv_i2c_write(0x50, 255);
    print_separator();

    /* ===== BROM Module ===== */
    print_test_header("BROM Module Tests");
    brom_boot_execute();
    print_separator();
    brom_boot_check();
    print_separator();
    brom_loader_load();
    print_separator();
    brom_loader_verify();
    print_separator();
    brom_loader_jump();
    print_separator();

    /* ===== Test Dynamic Module Control ===== */
    print_test_header("Dynamic Module Control Test");
    printf("Disabling DEMO module...\n");
    g_ww_log_mod_enable[WW_LOG_MOD_DEMO] = 0;
    demo_process(99);  /* Should not output */
    printf("Re-enabling DEMO module...\n");
    g_ww_log_mod_enable[WW_LOG_MOD_DEMO] = 1;
    demo_process(100);  /* Should output */
    print_separator();

    /* ===== Test Dynamic Level Control ===== */
    print_test_header("Dynamic Level Control Test");
    printf("Setting log level to ERR only...\n");
    g_ww_log_level_threshold = WW_LOG_LEVEL_ERR;
    test_unit_run();  /* Only errors should show */
    printf("Restoring log level to DBG...\n");
    g_ww_log_level_threshold = WW_LOG_LEVEL_DBG;
    print_separator();

#if defined(CONFIG_WW_LOG_ENCODE_MODE) && defined(CONFIG_WW_LOG_RAM_BUFFER_EN)
    /* ===== Dump RAM Buffer ===== */
    print_test_header("RAM Buffer Dump");
    ww_log_ram_dump();
#endif

    /* ===== Test Complete ===== */
    printf("\n");
    printf("=====================================\n");
    printf("  All Tests Completed\n");
    printf("=====================================\n\n");

    return 0;
}
