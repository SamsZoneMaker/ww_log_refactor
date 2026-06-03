/**
 * @file main.c
 * @brief ww_log v1 simulation driver.
 *
 * Exercises the unified LOG API through demo modules, then demonstrates the
 * dynamic switches (module mask + level threshold). main.c itself is NOT
 * registered in log_config.json, so its own LOG_* calls (if any) are off by
 * design -- all visible logs come from the registered demo modules.
 *
 * Build mode is selected in include/ww_log.h (STR / ENCODE / DISABLED).
 */

#include "ww_log.h"
#include "ww_log_ctrl.h"
#include "ww_log_config.h"
#include <stdio.h>

#if (WW_LOG_BACKEND_RAM == 1) || (WW_LOG_BACKEND_STORAGE == 1)
#include "ww_log_store.h"
#endif
#ifdef SIMULATION_MODE
#include "ww_log_panic.h"   /* ww_log_ram_dump_file / ww_log_storage_dump_file */
#endif

/* DEMO module */
extern void demo_init(void);
extern void demo_process(int task_id);

/* DRIVERS module */
extern void drv_uart_init(void);
extern void drv_uart_send(int length);

/* TEST module (statically disabled) */
extern void test_unit_run(void);

static void banner(const char *title)
{
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

int main(void)
{
    printf("\n=======================================\n");
    printf("  ww_log v1 Simulation\n");
    printf("=======================================\n");
#if defined(WW_LOG_MODE_DISABLED)
    printf("  Mode: DISABLED\n");
#elif defined(WW_LOG_MODE_STR)
    printf("  Mode: STRING\n");
#elif defined(WW_LOG_MODE_ENCODE)
    printf("  Mode: ENCODE\n");
#endif
    printf("  Level threshold: %d (runtime configurable)\n",
           ww_log_get_level_threshold());
    printf("=======================================\n");

    ww_log_init();

    /* ===== Normal logging through registered modules ===== */
    banner("DEMO module");
    demo_init();
    demo_process(42);

    banner("DRIVERS module");
    drv_uart_init();
    drv_uart_send(128);

    banner("TEST module (statically disabled)");
    test_unit_run();

    /* ===== Dynamic switch: disable a module at runtime ===== */
    banner("Dynamic switch: disable DEMO (id=1)");
    ww_log_disable_module(1);
    printf("-- demo_init() below should produce NO demo logs --\n");
    demo_init();
    ww_log_enable_module(1);

    /* ===== Dynamic switch: raise level threshold to ERR ===== */
    banner("Dynamic switch: level threshold = ERR");
    ww_log_set_level_threshold(WW_LOG_LEVEL_ERR);
    printf("-- only ERR lines should appear below --\n");
    demo_process(-1);   /* hits LOG_ERR + early return */
    drv_uart_send(512); /* WRN/INF/DBG suppressed, none are ERR */
    ww_log_set_level_threshold(WW_LOG_LEVEL_DBG);

    /* ===== panic mode: bypass filters + force-flush surviving logs ===== */
    banner("Panic mode (crash handler)");
    printf("-- threshold still ERR, but panic bypasses all filtering --\n");
    ww_log_set_level_threshold(WW_LOG_LEVEL_ERR);
    ww_log_disable_module(1);   /* DEMO off: would normally drop these */
    ww_log_panic();             /* HardFault/watchdog would call this */
    demo_process(7);            /* INF/DBG now emitted despite filters */
    ww_log_enable_module(1);
    ww_log_set_level_threshold(WW_LOG_LEVEL_DBG);

#if (WW_LOG_BACKEND_RAM == 1)
    banner("RAM backend status");
    printf("RAM usage: %u bytes, available: %u bytes\n",
           log_ram_get_usage(), log_ram_get_available());
#ifdef SIMULATION_MODE
    /* Dump the RAM region to files (host would do this over JTAG on target). */
    ww_log_ram_dump_file("ram_dump.bin", WW_LOG_DUMP_BIN);
    ww_log_ram_dump_file("ram_dump.hex", WW_LOG_DUMP_HEX);
    printf("RAM dumped -> ram_dump.bin / ram_dump.hex\n");
#endif
#if (WW_LOG_BACKEND_STORAGE == 1)
    printf("Forcing final flush...\n");
    log_flush_now();
    printf("RAM usage after flush: %u bytes\n", log_ram_get_usage());
#ifdef SIMULATION_MODE
    ww_log_storage_dump_file("storage_dump.bin", WW_LOG_DUMP_BIN);
    ww_log_storage_dump_file("storage_dump.hex", WW_LOG_DUMP_HEX);
    printf("Storage dumped -> storage_dump.bin / storage_dump.hex\n");
#endif
#endif
    log_ram_dump_hex();
#endif

    printf("\n=======================================\n");
    printf("  Done\n");
    printf("=======================================\n\n");
    return 0;
}
