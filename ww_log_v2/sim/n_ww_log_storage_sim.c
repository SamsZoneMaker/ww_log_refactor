/**
 * @file n_ww_log_storage_sim.c
 * @brief Sim replacement for log/n_ww_log_storage.c.
 *
 * The original file cannot be compiled as-is because it:
 *   1. Contains a bare "#include <>" (syntax error, placeholder artifact)
 *   2. Unconditionally includes FW-specific headers (FreeRTOS, flash, EEPROM)
 *
 * This file implements the same RAM-buffer functions with identical logic,
 * using only sim-safe includes.  CONFIG_N_LOG_BACKEND_EXT_MEM is NOT defined
 * in the sim, so all ext-mem API stubs simply return WW_ERR / WW_FALSE.
 *
 * Logic is kept bit-for-bit identical to n_ww_log_storage.c to ensure any
 * future diff-review is clean.
 */

#include "autoconf.h"       /* must be first: guards in log_task.h/log_storage.h */
#include "ww_std.h"
#include "log/n_ww_log_storage.h"
#include "log/n_ww_log_control.h"
#include "log/n_ww_log_task.h"

/* ====================================================================== */

static LOG_RAM_BUFFER_T g_ram_buffer = {0};

/* ====================================================================== */
/* Static helpers                                                           */
/* ====================================================================== */

static void init_header(LOG_RAM_HEADER_T *header)
{
    ww_memset(header, 0, sizeof(LOG_RAM_HEADER_T));

    header->magic          = LOG_RAM_MAGIC;
    header->write_index    = 0;
    header->read_index     = 0;
    header->pending_len    = 0;
    header->flush_count    = 0;
    header->flags          = 0;
    header->log_count      = 0;
    header->overflow_count = 0;
    header->reserved1      = 0;
    header->reserved2      = 0;

    header->checksum = LOG_CALC_STRUCT_CHECKSUM(header);
}

static WW_RTN validate_header(const LOG_RAM_HEADER_T *header)
{
    N_RETURN_CODE_IF_TRUE(header->magic != LOG_RAM_MAGIC, WW_ERR);

    N_RETURN_CODE_IF_TRUE(header->write_index >= LOG_RAM_DATA_SIZE ||
                          header->read_index  >= LOG_RAM_DATA_SIZE, WW_ERR);

    N_RETURN_CODE_IF_TRUE(header->pending_len > LOG_RAM_DATA_SIZE, WW_ERR);

    U32 calculated = LOG_CALC_STRUCT_CHECKSUM(header);
    N_RETURN_CODE_IF_TRUE(calculated != header->checksum, WW_ERR);

    return WW_OK;
}

/* ====================================================================== */
/* Public RAM API                                                           */
/* ====================================================================== */

void log_ram_init(bool force_clear)
{
    g_ram_buffer.header    = (LOG_RAM_HEADER_T *)DLM_MAINTAIN_LOG_BASE_ADDR;
    g_ram_buffer.data      = (U8 *)(DLM_MAINTAIN_LOG_BASE_ADDR + LOG_RAM_HEADER_SIZE);
    g_ram_buffer.data_size = LOG_RAM_DATA_SIZE;
    g_ram_buffer.threshold = LOG_RAM_FLUSH_THRESHOLD;

    WW_RTN rtn = 0;

    if (force_clear == WW_DISABLE)
    {
        rtn = validate_header(g_ram_buffer.header);
        if (rtn == WW_OK && log_ram_validate_data() != WW_OK)
        {
            g_ram_buffer.header->flags |= LOG_FLAG_CORRUPTED;
            g_ram_buffer.header->checksum = LOG_CALC_STRUCT_CHECKSUM(g_ram_buffer.header);
            rtn = WW_ERR;
        }
    }

    if (rtn != WW_OK || force_clear == WW_ENABLE)
    {
        init_header(g_ram_buffer.header);
        ww_memset(g_ram_buffer.data, 0, g_ram_buffer.data_size);
    }

    g_ram_buffer.header->log_count = 0;
    g_ram_buffer.header->checksum  = LOG_CALC_STRUCT_CHECKSUM(g_ram_buffer.header);

    ww_printf("[LOG][RAM]: Initialized (force_clear=%u)\n", force_clear);
}

void log_ram_get_header_info(LOG_RAM_HEADER_T *info)
{
    N_RETURN_IF_TRUE(info == NULL, WW_ERR);

    info->magic       = g_ram_buffer.header->magic;
    info->write_index = g_ram_buffer.header->write_index;
    info->read_index  = g_ram_buffer.header->read_index;
    info->pending_len = g_ram_buffer.header->pending_len;
    info->flush_count = g_ram_buffer.header->flush_count;
    info->flags       = g_ram_buffer.header->flags;
}

U32 log_calc_checksum(const void *data, U32 len)
{
    U32 sum = 0;
    const U32 *p = (const U32 *)data;

    for (U32 i = 0; i < len / sizeof(U32); i++)
    {
        sum += p[i];
    }
    return sum;
}

static void log_ram_drop_oldest(LOG_RAM_HEADER_T *header)
{
    U32 old_hdr  = *(U32 *)(g_ram_buffer.data + header->read_index);
    U8  old_pcnt = (U8)N_WW_LOG_PCNT_OF(old_hdr);
    U16 old_size = 4 + (U16)old_pcnt * 4;
    U16 ri = header->read_index;
    U16 b;

    for (b = 0; b < old_size; b += 4)
    {
        ri = log_ram_get_next_index(ri);
    }
    header->read_index = ri;

    if (header->overflow_count < 0xFFFF)
    {
        header->overflow_count++;
    }
    header->flags |= LOG_FLAG_OVERFLOW;
}

WW_RTN log_ram_write(U32 encoded, U32 *params, U8 param_count)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 write_idx;
    U16 required = 4 + (U16)param_count * 4;
    U8  i;

    if (required > g_ram_buffer.data_size - 1)
    {
        return WW_OK;
    }

    if (log_mutex_lock() != WW_OK)
    {
        return LOG_EXT_ERR_MUTEX_FAIL;
    }

    while (log_ram_get_available() < required)
    {
        log_ram_drop_oldest(header);
    }

    write_idx = header->write_index;
    *(U32 *)(g_ram_buffer.data + write_idx) = encoded;
    write_idx = log_ram_get_next_index(write_idx);

    for (i = 0; i < param_count; i++)
    {
        *(U32 *)(g_ram_buffer.data + write_idx) = params[i];
        write_idx = log_ram_get_next_index(write_idx);
    }

    header->write_index = write_idx;
    header->log_count  += 1;
    header->checksum    = LOG_CALC_STRUCT_CHECKSUM(header);

    log_mutex_unlock();
    return WW_OK;
}

void log_ram_mark_error(void)
{
    g_ram_buffer.header->flags |= LOG_FLAG_ERROR;
    g_ram_buffer.header->checksum = LOG_CALC_STRUCT_CHECKSUM(g_ram_buffer.header);
}

WW_RTN log_ram_validate_data(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 idx    = header->read_index;
    U16 used   = get_current_usage();
    U16 walked = 0;

    while (walked < used)
    {
        U32 hdr  = *(U32 *)(g_ram_buffer.data + idx);
        U8  pcnt = (U8)N_WW_LOG_PCNT_OF(hdr);
        U16 size = 4 + (U16)pcnt * 4;
        U16 b;

        if (walked + size > used)
        {
            return WW_ERR;
        }
        for (b = 0; b < size; b += 4)
        {
            idx = log_ram_get_next_index(idx);
        }
        walked += size;
    }

    return (idx == header->write_index) ? WW_OK : WW_ERR;
}

void log_ram_dump_hex(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16  usage;
    U8  *base = (U8 *)header;

    if (header->flush_count > 0)
    {
        usage = LOG_RAM_HEADER_SIZE + LOG_RAM_DATA_SIZE;
        ww_printf("[LOG][RAM] RAM Buffer: FULL DUMP (flush_count=%u), write_index=%u\n",
                  header->flush_count, header->write_index);
    }
    else
    {
        usage = header->write_index;
        ww_printf("[LOG][RAM] RAM Buffer: ACTIVE DATA (write_index=%u)\n",
                  header->write_index);
    }

    ww_printf("\n========= RAM LOG DUMP =========\n");
    ww_printf("--- Header ---\n");
    ww_printf("Magic:       0x%08X %s\n", header->magic,
              (header->magic == LOG_RAM_MAGIC) ? "(VALID)" : "(INVALID)");
    ww_printf("Write Index: %u bytes\n", header->write_index);
    ww_printf("Log Count:   %u\n", header->log_count);
    ww_printf("\n--- Data ---\n");
    ww_printf("Data Size:   %u bytes\n", g_ram_buffer.data_size);

    if (usage == 0)
    {
        ww_printf("(Empty)\n================================\n");
        return;
    }

    usage = (header->flush_count > 0)
            ? (LOG_RAM_HEADER_SIZE + LOG_RAM_DATA_SIZE)
            : (LOG_RAM_HEADER_SIZE + header->write_index);

    ww_printf("Offset | Bytes       | DWORD\n");
    ww_printf("-------------------------------\n");
    for (U16 i = 0; i < usage; i += 4)
    {
        ww_printf("0x%04X | ", i);
        for (U16 j = 0; j < 4; j++)
        {
            if (i + j < usage) ww_printf("%02X ", base[i + j]);
            else               ww_printf("   ");
        }
        ww_printf("| ");
        U32 val = 0;
        for (U16 j = 0; j < 4 && (i + j) < usage; j++)
            val |= ((U32)base[i + j]) << (j * 8);
        ww_printf("0x%08X\n", val);
    }
    ww_printf("================================\n");
    ww_printf("sizeof(LOG_RAM_HEADER_T) = %u\n", (U32)sizeof(LOG_RAM_HEADER_T));
}

void log_ram_index_reset(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;

    header->write_index = 0;
    header->read_index  = 0;
    header->flags       = 0;
    header->pending_len = 0;
    header->checksum    = LOG_CALC_STRUCT_CHECKSUM(header);
}

U8 log_ram_validate(void)
{
    return (U8)validate_header(g_ram_buffer.header);
}

U16 get_current_usage(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;

    if (header->write_index >= header->read_index)
    {
        return header->write_index - header->read_index;
    }
    return g_ram_buffer.data_size - header->read_index + header->write_index;
}

U16 log_ram_get_available(void)
{
    return (g_ram_buffer.data_size - 1) - get_current_usage();
}

U16 log_ram_get_data_size(void)    { return g_ram_buffer.data_size; }
U16 log_ram_get_write_index(void)  { return g_ram_buffer.header->write_index; }
U16 log_ram_get_read_index(void)   { return g_ram_buffer.header->read_index; }
U16 log_ram_get_pending_len(void)  { return g_ram_buffer.header->pending_len; }
U8 *log_ram_get_data_ptr(void)     { return g_ram_buffer.data; }
