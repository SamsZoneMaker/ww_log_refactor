/**
 * @file n_ww_log_output.c
 * @brief Log output: encode-mode impl, string-mode impl, and backend dispatch.
 */

#include "ww_std.h"
#include "log/n_ww_log_macro.h"   /* N_LOG and N_RETURN macros, pulls def + output */
#include "log/n_ww_log_api.h"     /* g_ww_log_module_mask / g_ww_log_level_threshold */

#if defined(CONFIG_N_LOG_BACKEND_RAM) || defined(CONFIG_N_LOG_BACKEND_EXT_MEM)
#include "log/n_ww_log_storage.h"
#endif


/* ==================== STRING mode output ==================== */
#if defined(CONFIG_N_LOG) && \
    (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_STRING) && \
    defined(CONFIG_N_LOG_BACKEND_UART)

#define N_WW_LOG_BUF_SIZE    (128)

static const char *level_names[] = {
    "ERR",  /* N_WW_LOG_LEVEL_ERR */
    "WRN",  /* N_WW_LOG_LEVEL_WRN */
    "INF",  /* N_WW_LOG_LEVEL_INF */
    "DBG",  /* N_WW_LOG_LEVEL_DBG */
};

void n_ww_log_str_output(U8 module_id, const char *filename, U32 line, U8 level,
                         const char *fmt, ...)
{
    /* Per-call storage keeps concurrent tasks from corrupting one shared line. */
    char ww_log_buf[N_WW_LOG_BUF_SIZE] = {0};
    va_list ap = {0};
    U32 header_len = 0;
    int remain_size = 0;
    int ret = 0;

    N_RETURN_IF_TRUE_WO_PRINT((g_ww_log_module_mask & (1U << module_id)) == WW_DISABLE, WW_DISABLE);
    N_RETURN_IF_TRUE_WO_PRINT(level > g_ww_log_level_threshold, WW_DISABLE);

    if (level > N_WW_LOG_LEVEL_DBG)
    {
        level = N_WW_LOG_LEVEL_DBG;
    }

    if (!fmt)
    {
        return;
    }

    header_len = ww_snprintf(ww_log_buf, sizeof(ww_log_buf),
                             "[%s] %s:%u - ", level_names[level], filename, line);

    if (header_len <= 0 || header_len >= sizeof(ww_log_buf))
    {
        return;
    }

    remain_size = sizeof(ww_log_buf) - header_len;

    va_start(ap, fmt);
    ret = ww_vsnprintf(ww_log_buf + header_len, remain_size,
                       LINESEP_FORMAT_WINDOWS, fmt, ap);
    va_end(ap);

    if (ret)
    {
        ww_printf("%s\n", ww_log_buf);
    }
}

#elif defined(CONFIG_N_LOG) && \
      (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_STRING)

/* STRING has no RAM wire representation. With UART disabled it is therefore a
 * deliberate no-op, matching the backend selection instead of printing anyway. */
void n_ww_log_str_output(U8 module_id, const char *filename, U32 line, U8 level,
                         const char *fmt, ...)
{
    (void)module_id;
    (void)filename;
    (void)line;
    (void)level;
    (void)fmt;
}

#endif /* string mode */


/* ==================== ENCODE mode output ==================== */
#if defined(CONFIG_N_LOG) && (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE)

void n_ww_log_encode_output(U16 file_id, U16 line, U8 level, U8 param_count, ...)
{
    U32 encoded;
    U32 params[N_WW_LOG_ENCODE_MAX_PARAMS];
    va_list args = {0};
    U8 i;
    U8 module_id = (U8)N_WW_LOG_MODULE_OF(file_id);

    /* Check module enable and level threshold (dynamic switch) */
    N_RETURN_IF_TRUE_WO_PRINT((g_ww_log_module_mask & (1U << module_id)) == WW_DISABLE, WW_DISABLE);
    N_RETURN_IF_TRUE_WO_PRINT(level > g_ww_log_level_threshold, WW_DISABLE);

    if (param_count > N_WW_LOG_ENCODE_MAX_PARAMS)
    {
        param_count = N_WW_LOG_ENCODE_MAX_PARAMS;
    }

    if (param_count > 0)
    {
        va_start(args, param_count);
        for (i = 0; i < param_count; i++)
        {
            params[i] = va_arg(args, U32);
        }
        va_end(args);
    }

    /* level is encoded into the entry header so the flush path can filter, per
     * entry, which entries reach external storage; it is still passed alongside
     * for the UART path and the boot-wide ERR flag. */
    encoded = N_WW_LOG_ENCODE(file_id, line, level, param_count);

    ww_log_backend_emit(encoded, params, param_count, level);
}

#endif /* encode mode */


/* ============================================================
 * Backend dispatch (encode mode, always compiled)
 * ============================================================ */

#ifdef CONFIG_N_LOG_BACKEND_UART
static void backend_uart_emit(U32 encoded, const U32 *params, U8 param_count)
{
    U8 i;
    ww_printf("0x%08X", encoded);
    for (i = 0; i < param_count; i++)
    {
        ww_printf(" 0x%08X", params[i]);
    }
    ww_printf("\n");

#ifndef __riscv
    fflush(stdout);
#endif
}
#endif

#ifdef CONFIG_N_LOG_BACKEND_RAM
static void backend_ram_emit(U32 encoded, const U32 *params, U8 param_count)
{
    /* The RAM ring keeps EVERY entry that passed the runtime filter (all levels);
     * the level->ext filter is applied later in the flush path (it reads level
     * back out of each entry header), so nothing is dropped here. */
    (void)log_ram_write(encoded, params, param_count);
}
#endif

void ww_log_backend_emit(U32 encoded, const U32 *params, U8 param_count, U8 level)
{
#ifdef CONFIG_N_LOG_BACKEND_UART
    /* UART gets every entry that passed the runtime level/module filter,
     * regardless of the ext-persist threshold. */
    backend_uart_emit(encoded, params, param_count);
#endif
#ifdef CONFIG_N_LOG_BACKEND_RAM
    backend_ram_emit(encoded, params, param_count);
#endif

    (void)encoded; (void)params; (void)param_count; (void)level;
}
