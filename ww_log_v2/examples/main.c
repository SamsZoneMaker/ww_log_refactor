/**
 * @file main.c
 * @brief ww_log v2 simulation driver.
 *
 * Exercises the unified N_LOG_* API through registered demo modules, then
 * demonstrates dynamic switches (module mask + level threshold).
 *
 * main.c itself is NOT registered in log_config.json, so its own N_LOG_*
 * calls (if any) are statically off by design.  All visible logs come from
 * the registered modules (DEMO, DRIVERS).
 *
 * Log mode is selected in sim/autoconf.h.
 */

#include "log/n_ww_log.h"
#include <stdio.h>

#if (CONFIG_N_LOG_BACKEND_RAM == 1)
#include "log/n_ww_log_storage.h"
#endif
#if (CONFIG_N_LOG_BACKEND_EXT_MEM == 1)
#include "sim_ext_storage.h"
#endif

/* Demo modules */
extern void demo_init(void);
extern void demo_process(int task_id);
extern void demo_burst(int n);

/* Driver module */
extern void drv_uart_init(void);
extern void drv_uart_send(int length);

/* Self-test suite (TEST module) */
extern int test_log_run_all(void);

static void banner(const char *title)
{
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

int main(void)
{
    printf("\n=======================================\n");
    printf("  ww_log v2 Simulation\n");
    printf("=======================================\n");

#if defined(CONFIG_N_LOG_MODE_DISABLED)
    printf("  Mode: DISABLED\n");
#elif defined(CONFIG_N_LOG_MODE_STRING)
    printf("  Mode: STRING\n");
#elif defined(CONFIG_N_LOG_MODE_ENCODE)
    printf("  Mode: ENCODE\n");
#else
    printf("  Mode: (unknown - check autoconf.h)\n");
#endif

#if (CONFIG_N_LOG_BACKEND_UART == 1)
    printf("  Backend: UART");
#endif
#if (CONFIG_N_LOG_BACKEND_RAM == 1)
    printf(" RAM");
#endif
    printf("\n  Level threshold: %u (runtime configurable)\n",
           n_ww_log_get_level_threshold());
    printf("=======================================\n\n");

    /* ===== Init log system ===== */
    n_ww_log_init();

    /* ===== Self-test suite ===== */
    banner("Self-test suite");
    {
        int failed = test_log_run_all();
        printf("\n[main] self-test reported %d failure(s)\n", failed);
    }

    /* ===== Normal logging through registered modules ===== */
    banner("DEMO module");
    demo_init();
    demo_process(42);

    banner("DRIVERS module");
    drv_uart_init();
    drv_uart_send(128);

    /* ===== Dynamic switch: disable a module at runtime ===== */
    banner("Dynamic switch: disable DEMO (id=1)");
    n_ww_log_disable_module(1);
    printf("-- demo_init() below should produce NO demo logs --\n");
    demo_init();
    n_ww_log_enable_module(1);

    /* ===== Dynamic switch: raise level threshold to ERR only ===== */
    banner("Dynamic switch: level threshold = ERR");
    n_ww_log_set_level_threshold(N_WW_LOG_LEVEL_ERR);
    printf("-- only ERR lines should appear below --\n");
    demo_process(-1);    /* triggers LOG_ERR + early return */
    drv_uart_send(512);  /* WRN/INF/DBG suppressed, none are ERR */
    n_ww_log_set_level_threshold(N_WW_LOG_LEVEL_DBG);

#if (CONFIG_N_LOG_BACKEND_RAM == 1)
    banner("RAM backend status");
    printf("Write index : %u bytes\n", log_ram_get_write_index());
    printf("Available   : %u bytes\n", log_ram_get_available());
#endif

#if (CONFIG_N_LOG_BACKEND_EXT_MEM == 1)
    /* ===== External-storage append-log flush test (encode mode) ===== */
    banner("EXT storage: append-log flush");
    log_ext_mem_available();   /* trigger lazy ext init so geometry is known */
    printf("Partition: %u B (8B 'XLOG' header + append stream), write_off=0x%X\n",
           (unsigned)log_ext_get_log_size(), log_ext_get_write_offset());

    /* Generate a burst; only ERR/WRN entries are persisted to ext by default,
     * so mix in a few ERR-producing calls -- otherwise the dumped archive is
     * empty and there is nothing to demonstrate decoding on. */
    demo_burst(400);
    for (int e = 0; e < 5; e++)
    {
        demo_process(-1);          /* takes the LOG_ERR + early-return path */
    }

    /* The sim has no running flush task, so drain explicitly. */
    {
        int passes = 0;
        while (log_ram_get_pending_len() > 0)
        {
            if (log_ram_flush() != 0)
            {
                printf("flush returned error, stopping\n");
                break;
            }
            passes++;
        }
        printf("Drained in %d passes; write_off=0x%X\n",
               passes, log_ext_get_write_offset());
        printf("Ext used=%u / %u B, RAM pending=%u\n",
               log_ext_mem_get_used(), log_ext_get_log_size(),
               log_ram_get_pending_len());
    }

    sim_ext_dump_partition("ext_dump.bin");
    sim_ext_dump_chip("ext_chip.bin");   /* whole device, partition table included */
    printf("Ext LOG partition dumped -> ext_dump.bin (decode with log_decoder.py)\n");
#endif

#if (CONFIG_N_LOG_BACKEND_RAM == 1)
    log_ram_dump_hex();
#endif

    printf("\n=======================================\n");
    printf("  Done\n");
    printf("=======================================\n\n");
    return 0;
}
