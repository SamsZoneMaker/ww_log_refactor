/**
 * @file n_ww_log_output.c
 * @brief Log output: encode-mode impl, string-mode impl, and backend dispatch.
 */

#include "ww_std.h"
#include "log/n_ww_log_output.h"

// Optional: because n_ww_log_control.h is included by n_ww_log_output.h
// #include "log/n_ww_log_control.h"

#if (CONFIG_N_LOG_BACKEND_RAM == 1) || (CONFIG_N_LOG_BACKEND_EXT_MEM == 1)
#include "log/n_ww_log_storage.h"
#endif


/* ==================== STRING mode output ==================== */
#if defined(CONFIG_N_LOG_MODE_STRING)

#define N_WW_LOG_BUF_SIZE    (128)

static const char *level_names[] = {
    "ERR",  /* N_WW_LOG_LEVEL_ERR */
    "WRN",  /* N_WW_LOG_LEVEL_WRN */
    "INF",  /* N_WW_LOG_LEVEL_INF */
    "DBG",  /* N_WW_LOG_LEVEL_DBG */
};

static char s_wwLogBuf[N_WW_LOG_BUF_SIZE] = {0};

void n_ww_log_str_output(U8 module_id, const char *filename, U32 line, U8 level,
                         const char *fmt, ...)
{
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

    header_len = ww_snprintf(s_wwLogBuf, sizeof(s_wwLogBuf),
                             "[%s] %s:%u - ", level_names[level], filename, line);

    if (header_len <= 0 || header_len >= sizeof(s_wwLogBuf))
    {
        // ww_printf("Failed to output: Header len too long\n");
        return;
    }

    remain_size = sizeof(s_wwLogBuf) - header_len;

    va_start(ap, fmt);
    ret = ww_vsnprintf(s_wwLogBuf + header_len, remain_size,
                       LINESEP_FORMAT_WINDOWS, fmt, ap);
    va_end(ap);

    if (ret)
    {
        ww_printf("%s\n", s_wwLogBuf);
    }
}

#endif /* CONFIG_N_LOG_MODE_STRING */


/* ==================== ENCODE mode output ==================== */
#ifdef CONFIG_N_LOG_MODE_ENCODE

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

    /* level is intentionally NOT encoded (see CLAUDE.md §2); it is passed to the
     * backend dispatch so the RAM/storage backend can apply the storage-persist
     * threshold and the ERR flag (level cannot be recovered from the bytes). */
    encoded = N_WW_LOG_ENCODE(file_id, line, param_count);

    ww_log_backend_emit(encoded, params, param_count, level);
}

#endif /* CONFIG_N_LOG_MODE_ENCODE */


/* ============================================================
 * Backend dispatch (encode mode, always compiled)
 * ============================================================ */

#if (CONFIG_N_LOG_BACKEND_UART == 1)
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

#if (CONFIG_N_LOG_BACKEND_RAM == 1)
static void backend_ram_emit(U32 encoded, const U32 *params, U8 param_count, U8 level)
{
    /* Storage-persist filter: entries above the threshold (default: DBG) are not
     * written to RAM / external storage. level is not encoded, so it must be
     * filtered here at emit time -> RAM content == storage content. */
    if (level > N_WW_LOG_STORAGE_THRESHOLD)
    {
        return;
    }

    (void)log_ram_write(encoded, (U32 *)params, param_count);

    /* Cheap "did anything bad happen this boot" signal for the host. */
    if (level == N_WW_LOG_LEVEL_ERR)
    {
        log_ram_mark_error();
    }
}
#endif

void ww_log_backend_emit(U32 encoded, const U32 *params, U8 param_count, U8 level)
{
#if (CONFIG_N_LOG_BACKEND_UART == 1)
    /* UART gets every entry that passed the runtime level/module filter,
     * regardless of the storage-persist threshold. */
    backend_uart_emit(encoded, params, param_count);
#endif
#if (CONFIG_N_LOG_BACKEND_RAM == 1)
    backend_ram_emit(encoded, params, param_count, level);
#endif

    (void)encoded; (void)params; (void)param_count; (void)level;
}
