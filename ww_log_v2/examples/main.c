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
#include "log/n_ww_log_control.h"
#include <stdio.h>

#if (CONFIG_N_LOG_BACKEND_RAM == 1)
#include "log/n_ww_log_storage.h"
#endif

/* Demo modules */
extern void demo_init(void);
extern void demo_process(int task_id);

/* Driver module */
extern void drv_uart_init(void);
extern void drv_uart_send(int length);

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
    log_ram_dump_hex();
#endif

    printf("\n=======================================\n");
    printf("  Done\n");
    printf("=======================================\n\n");
    return 0;
}
