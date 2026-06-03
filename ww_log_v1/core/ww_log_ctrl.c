/**
 * @file ww_log_ctrl.c
 * @brief Runtime control (module mask, level threshold) and system init.
 */

#include "ww_log.h"
#include "ww_log_ctrl.h"
#include "ww_log_config.h"
#include <stdio.h>

#if (WW_LOG_BACKEND_RAM == 1) || (WW_LOG_BACKEND_STORAGE == 1)
#include "ww_log_store.h"
#endif

#ifdef SIMULATION_MODE
#include "sim_storage.h"
#endif

/* ============================================================
 * Dynamic module mask
 * ============================================================ */

U32 g_ww_log_module_mask = 0xFFFFFFFF;   /* all modules enabled */

void ww_log_set_module_mask(U32 mask)    { g_ww_log_module_mask = mask; }
U32  ww_log_get_module_mask(void)        { return g_ww_log_module_mask; }

void ww_log_enable_module(U8 module_id)
{
    if (module_id < WW_LOG_MODULE_MAX)
        g_ww_log_module_mask |= (1U << module_id);
}

void ww_log_disable_module(U8 module_id)
{
    if (module_id < WW_LOG_MODULE_MAX)
        g_ww_log_module_mask &= ~(1U << module_id);
}

U8 ww_log_is_module_enabled(U8 module_id)
{
    if (module_id >= WW_LOG_MODULE_MAX) return 0;
    return (g_ww_log_module_mask & (1U << module_id)) ? 1 : 0;
}

/* ============================================================
 * Level threshold
 * ============================================================ */

U8 g_ww_log_level_threshold = WW_LOG_LEVEL_DBG;   /* allow all by default */

void ww_log_set_level_threshold(U8 level)
{
    if (level <= WW_LOG_LEVEL_DBG)
        g_ww_log_level_threshold = level;
}

U8 ww_log_get_level_threshold(void) { return g_ww_log_level_threshold; }

/* ============================================================
 * System init
 * ============================================================ */

void ww_log_init(void)
{
    printf("LOG: ww_log v1 init (mode: ");
#if defined(WW_LOG_MODE_ENCODE)
    printf("ENCODE");
#elif defined(WW_LOG_MODE_STR)
    printf("STRING");
#elif defined(WW_LOG_MODE_DISABLED)
    printf("DISABLED");
#else
    printf("UNKNOWN");
#endif
    printf(", backends:");
#if (WW_LOG_BACKEND_UART == 1)
    printf(" UART");
#endif
#if (WW_LOG_BACKEND_RAM == 1)
    printf(" RAM");
#endif
#if (WW_LOG_BACKEND_STORAGE == 1)
    printf(" STORAGE");
#endif
    printf(")\n");

#if (WW_LOG_BACKEND_RAM == 1)
    log_ram_init(0);   /* try to preserve existing data (hot restart) */
#endif
#if (WW_LOG_BACKEND_STORAGE == 1)
#ifdef SIMULATION_MODE
    sim_storage_init();   /* must run before log_storage_init() on PC */
#endif
    log_storage_init();
    log_header_init();
    log_flush_init();
#endif
}
