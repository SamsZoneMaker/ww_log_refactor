/**
 * @file ww_log_output.c
 * @brief Log output: encode-mode impl, string-mode impl, and backend dispatch.
 */

#include "ww_log.h"
#include "ww_log_output.h"
#include "ww_log_panic.h"
#include "ww_log_config.h"
#include <stdio.h>
#include <stdarg.h>

#if (WW_LOG_BACKEND_RAM == 1) || (WW_LOG_BACKEND_STORAGE == 1)
#include "ww_log_store.h"
#endif

/* ============================================================
 * ENCODE mode output
 * ============================================================ */
#ifdef WW_LOG_MODE_ENCODE

#define WW_LOG_ENCODE_MAX_PARAMS  16

void ww_log_encode_output(U16 file_id, U16 line, U8 level, U8 param_count, ...)
{
    U32 encoded;
    U32 params[WW_LOG_ENCODE_MAX_PARAMS];
    va_list args;
    U8 i;
    U8 module_id = (U8)WW_LOG_MODULE_OF(file_id);

    /* Panic mode bypasses all filtering (CLAUDE.md §6.1). */
    if (!g_ww_log_panic_flag) {
        if ((g_ww_log_module_mask & (1U << module_id)) == 0) return;
        if (level > g_ww_log_level_threshold) return;
    }

    if (param_count > WW_LOG_ENCODE_MAX_PARAMS)
        param_count = WW_LOG_ENCODE_MAX_PARAMS;

    if (param_count > 0) {
        va_start(args, param_count);
        for (i = 0; i < param_count; i++)
            params[i] = va_arg(args, U32);
        va_end(args);
    }

    /* level is intentionally NOT encoded (see CLAUDE.md §2) */
    encoded = WW_LOG_ENCODE(file_id, line, param_count);

    ww_log_backend_emit(encoded, params, param_count, g_ww_log_panic_flag);
}

#endif /* WW_LOG_MODE_ENCODE */

/* ============================================================
 * STRING mode output
 * ============================================================ */
#ifdef WW_LOG_MODE_STR

static const char *level_names[] = {
    "ERR",  /* WW_LOG_LEVEL_ERR */
    "WRN",  /* WW_LOG_LEVEL_WRN */
    "INF",  /* WW_LOG_LEVEL_INF */
    "DBG",  /* WW_LOG_LEVEL_DBG */
};

void ww_log_str_output(U8 module_id, const char *filename, U32 line, U8 level,
                       const char *fmt, ...)
{
    va_list args;

    /* Panic mode bypasses all filtering (CLAUDE.md §6.1). */
    if (!g_ww_log_panic_flag) {
        if ((g_ww_log_module_mask & (1U << module_id)) == 0) return;
        if (level > g_ww_log_level_threshold) return;
    }

    if (level > WW_LOG_LEVEL_DBG)
        level = WW_LOG_LEVEL_DBG;

    printf("[%s] %s:%u - ", level_names[level], filename, (unsigned)line);

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}

#endif /* WW_LOG_MODE_STR */

/* ============================================================
 * Backend dispatch (encode mode, always compiled)
 * ============================================================ */

#if (WW_LOG_BACKEND_UART == 1)
static void backend_uart_emit(U32 encoded, const U32 *params, U8 param_count)
{
    U8 i;
    printf("0x%08X", encoded);
    for (i = 0; i < param_count; i++)
        printf(" 0x%08X", params[i]);
    printf("\n");
    fflush(stdout);
}
#endif

#if (WW_LOG_BACKEND_RAM == 1)
static void backend_ram_emit(U32 encoded, const U32 *params, U8 param_count, U8 sync)
{
    int ret = log_ram_write(encoded, (U32 *)params, param_count);

#if (WW_LOG_BACKEND_STORAGE == 1)
    if (ret == 1 || sync) {
        if (log_flush_request(sync) == 0)
            log_flush_process();
    }
#else
    (void)ret;
    (void)sync;
#endif
}
#endif

void ww_log_backend_emit(U32 encoded, const U32 *params, U8 param_count, U8 sync)
{
#if (WW_LOG_BACKEND_UART == 1)
    backend_uart_emit(encoded, params, param_count);
#endif
#if (WW_LOG_BACKEND_RAM == 1)
    backend_ram_emit(encoded, params, param_count, sync);
#endif

#if (WW_LOG_BACKEND_UART == 0) && (WW_LOG_BACKEND_RAM == 0)
    (void)encoded; (void)params; (void)param_count;
#endif
    (void)sync;
}
