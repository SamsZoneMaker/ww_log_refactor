/*************************** description start ***************************/
/* ww_log RAM ring buffer.
 *
 * Owns the power-loss-retained log ring in the DLM maintain region: the
 * LOG_RAM_HEADER_T + data area at DLM_MAINTAIN_LOG_BASE_ADDR, the whole-entry
 * FIFO write/eviction path, structural validation and the read accessors.
 *
 * This file is the sole owner of g_ram_buffer; the external-storage block ring
 * (n_ww_log_storage.c) never touches it directly -- it drains the ring through
 * the log_ram_pack_ext() / log_ram_consume() helpers at the bottom of this
 * file, so the RAM state stays encapsulated here. */
/*************************** description end *****************************/

// #include <>
// #include ""
#include "ww_std.h"
#include "log/n_ww_log_storage.h"
#include "log/n_ww_log_macro.h"    /* N_RETURN_*_IF_TRUE + (via def) encode accessors */
#include "log/n_ww_log_task.h"

#ifdef CONFIG_N_LOG_BACKEND_RAM

/*************************** static variable start ***************************/
/* to be used only in this file */
static LOG_RAM_BUFFER_T g_ram_buffer = {0};
/*************************** static variable end *****************************/


/*************************** static function start ***************************/
/* to be used only in this file */

/**
 * @brief Initialize header with default values
 */
static void init_header(LOG_RAM_HEADER_T *header)
{
    ww_memset(header, 0, sizeof(LOG_RAM_HEADER_T));

    header->magic = LOG_RAM_MAGIC;
    header->write_index = 0;
    header->read_index = 0;
    header->pending_len = 0;
    header->flush_count = 0;
    header->flags = 0;
    header->log_count = 0;
    header->overflow_count = 0;
    header->reserved1 = 0;
    header->reserved2 = 0;

    header->checksum = LOG_CALC_STRUCT_CHECKSUM(header);
}

/**
 * @brief Validate header integrity
 */
static WW_RTN validate_header(const LOG_RAM_HEADER_T *header)
{
    N_RETURN_CODE_IF_TRUE(header->magic != LOG_RAM_MAGIC, WW_ERR);

    N_RETURN_CODE_IF_TRUE(header->write_index >= LOG_RAM_DATA_SIZE ||
                          header->read_index >= LOG_RAM_DATA_SIZE, WW_ERR);

    N_RETURN_CODE_IF_TRUE(header->pending_len > LOG_RAM_DATA_SIZE, WW_ERR);

    /* Check checksum */
    U32 calculated = LOG_CALC_STRUCT_CHECKSUM(header);

    N_RETURN_CODE_IF_TRUE(calculated != header->checksum, WW_ERR);

    return WW_OK;
}

/**
 * @brief Drop the single oldest whole entry at read_index (FIFO eviction).
 *        Parses the entry's param_count from its header to know its length,
 *        advances read_index past it (word-aligned wrap), and flags overflow.
 * @note  Caller must hold the lock. Assumes read_index points at a valid entry
 *        start, which the ring invariant guarantees.
 */
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

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
    if (header->pending_len >= old_size)
    {
        header->pending_len -= old_size;
    }
    else
    {
        header->pending_len = 0;
    }
#endif
}

/*************************** static function end *****************************/


/*************************** global function start ***************************/

void log_ram_init(bool force_clear)
{
    g_ram_buffer.header = (LOG_RAM_HEADER_T *)DLM_MAINTAIN_LOG_BASE_ADDR;
    g_ram_buffer.data = (U8 *)(DLM_MAINTAIN_LOG_BASE_ADDR + LOG_RAM_HEADER_SIZE);
    g_ram_buffer.data_size = LOG_RAM_DATA_SIZE;
    g_ram_buffer.threshold = LOG_RAM_FLUSH_THRESHOLD;

    WW_RTN rtn = 0;

    if (force_clear == WW_DISABLE)
    {
        /* Hot restart: header must pass AND the entry chain must be structurally
         * intact, otherwise we cannot trust the data region. */
        rtn = validate_header(g_ram_buffer.header);
        if (rtn == WW_OK && log_ram_validate_data() != WW_OK)
        {
            /* Header looks fine but the data chain is broken: keep the buffer
             * (for forensic inspection) but flag it so the host knows. */
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

    /* log_count is a per-boot counter (entries logged since this start). Reset it
     * on every init, including hot restart where the buffer contents are kept. */
    g_ram_buffer.header->log_count = 0;
    g_ram_buffer.header->checksum  = LOG_CALC_STRUCT_CHECKSUM(g_ram_buffer.header);

    ww_printf("[LOG][RAM]: Initialized (force_clear=%u)\n", force_clear);
}

void log_ram_get_header_info(LOG_RAM_HEADER_T *info)
{
    N_RETURN_IF_TRUE(info == NULL, WW_ERR);

    info->magic = g_ram_buffer.header->magic;
    info->write_index = g_ram_buffer.header->write_index;
    info->read_index = g_ram_buffer.header->read_index;
    info->pending_len = g_ram_buffer.header->pending_len;
    info->flush_count = g_ram_buffer.header->flush_count;
    info->flags = g_ram_buffer.header->flags;
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

WW_RTN log_ram_write(U32 encoded, U32 *params, U8 param_count)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 write_idx;
    U16 required = 4 + (U16)param_count * 4; /* encoded header + N params */
    U8  i;

    /* Not initialised yet (LOG fired before n_ww_log_init): g_ram_buffer.header
     * is NULL, so bail instead of dereferencing it. */
    if (header == NULL)
    {
        return WW_ERR;
    }

    /* An entry larger than the whole ring can never fit -> discard up front
     * (would otherwise loop forever evicting). */
    if (required > g_ram_buffer.data_size - 1)
    {
        return WW_OK;
    }

    if (log_mutex_lock() != WW_OK)
    {
        return LOG_EXT_ERR_MUTEX_FAIL;
    }

    /* Make room by evicting whole oldest entries until 'required' fits.
     * Whole-entry eviction keeps read_index on an entry boundary so the ring
     * never desyncs, and sets LOG_FLAG_OVERFLOW so the host knows data was lost.
     * (When the ring is empty available == data_size-1 >= required, so this
     * loop never runs and cannot spin.) */
    while (log_ram_get_available() < required)
    {
        log_ram_drop_oldest(header);
    }

    /* Write the entry word-by-word. data_size is a multiple of 4 and indices
     * step by 4, so no single U32 is ever split across the wrap boundary; an
     * entry may still wrap at WORD granularity, which the decoder reassembles
     * via the read/write pointers + overflow flag. */
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

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
    header->pending_len += required;
    /* Once the archive is frozen the flush task can only wake up and bail, so
     * stop signalling it; the ring keeps rolling for UART and RAM dumps. */
    if (log_ram_is_need_flush() == WW_TRUE && log_ext_mem_is_full() != WW_TRUE)
    {
        log_flush_notify();
    }
#endif

    header->checksum = LOG_CALC_STRUCT_CHECKSUM(header);

    log_mutex_unlock();
    return WW_OK;
}

/**
 * @brief Mark that an ERR-level entry was recorded this boot (LOG_FLAG_ERROR).
 *        level is not encoded into entries, so the emit layer calls this for
 *        ERR logs to give the host a cheap "did anything bad happen" signal.
 */
void log_ram_mark_error(void)
{
    g_ram_buffer.header->flags |= LOG_FLAG_ERROR;
    g_ram_buffer.header->checksum = LOG_CALC_STRUCT_CHECKSUM(g_ram_buffer.header);
}

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
/**
 * @brief Record that the external archive filled up (FREEZE policy).
 *        Kept in the RAM header rather than only in the ext ctx so it reaches a
 *        RAM dump and survives the cold boot that discards the ctx -- without
 *        it, an archive that stops mid-way looks the same as a device that
 *        simply stopped logging.
 * @note  Caller already holds the log mutex (flush path).
 */
void log_ram_set_ext_full(void)
{
    g_ram_buffer.header->flags |= LOG_FLAG_EXT_FULL;
    g_ram_buffer.header->checksum = LOG_CALC_STRUCT_CHECKSUM(g_ram_buffer.header);
}
#endif

/**
 * @brief Structural integrity check of the data area (not just the header).
 *        Walks every entry from read_index to write_index; if any entry's
 *        param_count would run the walk past write_index the data is corrupt.
 * @return WW_OK if the entry chain lands exactly on write_index, else WW_ERR.
 * @note  Cheap O(entries) walk, no stored CRC. On failure the caller sets
 *        LOG_FLAG_CORRUPTED so the host knows recovery was partial.
 */
WW_RTN log_ram_validate_data(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 idx   = header->read_index;
    U16 used  = get_current_usage();
    U16 walked = 0;

    while (walked < used)
    {
        U32 hdr  = *(U32 *)(g_ram_buffer.data + idx);
        U8  pcnt = (U8)N_WW_LOG_PCNT_OF(hdr);
        U16 size = 4 + (U16)pcnt * 4;
        U16 b;

        if (walked + size > used)
        {
            return WW_ERR;   /* entry runs past the written region -> corrupt */
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
    U16 usage;
    U8 *base = (U8 *)header;

    if (header->flush_count > 0)
    {
        usage = LOG_RAM_HEADER_SIZE + LOG_RAM_DATA_SIZE;
        ww_printf("[LOG][RAM] RAM Buffer: FULL DUMP (flush_count=%u), New write index=%u\n",
                  header->flush_count, header->write_index);
    }
    else
    {
        usage = header->write_index;
        ww_printf("[LOG][RAM] RAM Buffer: ACTIVE DATA (write_index=%u)\n",
                  header->write_index);
    }

    ww_printf("\n========= RAM LOG DUMP =========\n");

    /* Print Header Info */
    ww_printf("--- Header Info ---\n");
    ww_printf("Magic:       0x%08X %s\n", header->magic,
              (header->magic == LOG_RAM_MAGIC) ? "(VALID)" : "(INVALID)");
    ww_printf("Write Index: %u bytes\n", header->write_index);

    ww_printf("\n--- Data Area ---\n");
    ww_printf("Data Size:   %u bytes\n", g_ram_buffer.data_size);
    ww_printf("Usage:       %u bytes (%.1f%%)\n", usage,
              (float)usage * 100.0f / g_ram_buffer.data_size);
    ww_printf("Available:   %u bytes\n", g_ram_buffer.data_size - usage);
    ww_printf("--------------------------------\n");

    if (usage == 0)
    {
        ww_printf("(Empty)\n");
        ww_printf("================================\n");
        return;
    }

    /* Print out the info in RAM */
    ww_printf("Offset | Byte Stream | DWORD (32-bit)\n");
    ww_printf("-------------------------------------\n");

    if (header->flush_count > 0)
    {
        usage = LOG_RAM_HEADER_SIZE + LOG_RAM_DATA_SIZE;
    }
    else
    {
        usage = LOG_RAM_HEADER_SIZE + header->write_index;
    }

    for (U16 i = 0; i < usage; i += 4)
    {
        ww_printf("0x%04X | ", i); /* i 就是相对偏移 */

        for (U16 j = 0; j < 4; j++)
        {
            if (i + j < usage)
                ww_printf("%02X ", base[i + j]);
            else
                ww_printf("   ");
        }

        ww_printf("| ");

        if (i + 3 < usage)
        {
            U32 val = *(U32 *)(base + i);
            ww_printf("0x%08X", val);
        }
        else
        {
            U32 val = 0;
            for (U16 j = 0; j < 4 && (i + j) < usage; j++)
                val |= ((U32)base[i + j]) << (j * 8);
            ww_printf("0x%08X", val);
        }

        ww_printf("\n");
    }
    ww_printf("-------------------------------------\n");
    ww_printf("sizeof(LOG_RAM_HEADER_T) = %u\n", (U32)sizeof(LOG_RAM_HEADER_T));
    ww_printf("LOG_RAM_HEADER_SIZE = %u\n", (U32)LOG_RAM_HEADER_SIZE);
    ww_printf("=====================================\n");
}

void log_ram_index_reset(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;

    header->write_index = 0;
    header->read_index = 0;
    header->flags = 0;
    header->pending_len = 0;

    header->checksum = LOG_CALC_STRUCT_CHECKSUM(header);
}

/* ====== log_api for external use ====== */
U8 log_ram_validate(void)
{
    return validate_header(g_ram_buffer.header);
}

U16 get_current_usage(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;

    if (header->write_index >= header->read_index)
    {
        return header->write_index - header->read_index;
    }
    else
    {   /* Wrapped around */
        return g_ram_buffer.data_size - header->read_index + header->write_index;
    }
}

U16 log_ram_get_available(void)
{
    return (g_ram_buffer.data_size - 1) - get_current_usage();
}

U16 log_ram_get_data_size(void)
{
    return g_ram_buffer.data_size;
}

U16 log_ram_get_write_index(void)
{
    return g_ram_buffer.header->write_index;
}

U16 log_ram_get_read_index(void)
{
    return g_ram_buffer.header->read_index;
}

U16 log_ram_get_pending_len(void)
{
    return g_ram_buffer.header->pending_len;
}

U8* log_ram_get_data_ptr(void)
{
    return g_ram_buffer.data;
}

U32 log_ram_get_log_count(void)
{
    return g_ram_buffer.header->log_count;
}

U16 log_ram_get_overflow_count(void)
{
    return g_ram_buffer.header->overflow_count;
}

U16 log_ram_get_flags(void)
{
    return g_ram_buffer.header->flags;
}


#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

/* ============================================================================
 * RAM ring -> external storage bridge.
 *
 * These helpers let n_ww_log_storage.c drain the RAM ring without reaching into
 * g_ram_buffer. The flush driver calls (under the log mutex):
 *     packed = log_ram_pack_ext(stage, budget, &consumed); // filter whole entries
 *     ... reserve write_off, append `packed` bytes ...
 *     log_ram_consume(consumed);                            // advance RAM cursor
 * ========================================================================== */

/**
 * @brief Walk whole entries from the RAM ring (oldest first) up to `budget` RAM
 *        bytes, copying only the ones whose level passes the ext threshold into
 *        `dst`. RAM keeps every level; external storage keeps a filtered subset.
 * @param dst       destination buffer (must be >= `budget` bytes; the packed
 *                  subset is always <= the bytes walked <= budget).
 * @param budget    max RAM bytes to walk this call (bounds `dst` and per-call work).
 * @param consumed  out: RAM bytes walked (persisted + filtered-out), which the
 *                  caller passes to log_ram_consume() to advance the read cursor.
 * @return bytes copied into `dst` (the persisted subset), 0 if all filtered out.
 * @note  Read-only on the ring: does NOT advance read_index. The caller pairs
 *        this with log_ram_consume() under the same lock.
 */
U16 log_ram_pack_ext(U8 *dst, U16 budget, U16 *consumed)
{
    U16 ri     = g_ram_buffer.header->read_index;
    U16 used   = get_current_usage();
    U16 walked = 0;
    U16 packed = 0;

    while (walked < used)
    {
        U32 ehdr  = *(U32 *)(g_ram_buffer.data + ri);
        U8  pcnt  = (U8)N_WW_LOG_PCNT_OF(ehdr);
        U8  level = (U8)N_WW_LOG_LEVEL_OF(ehdr);
        U16 esz   = 4 + (U16)pcnt * 4;
        U16 b;

        if ((U32)walked + esz > budget)
        {
            break;                       /* keep the batch within budget */
        }

        if (level <= N_WW_LOG_EXT_LEVEL_THRESHOLD)
        {
            for (b = 0; b < esz; b += 4)
            {
                *(U32 *)(dst + packed + b) = *(U32 *)(g_ram_buffer.data + ri);
                ri = log_ram_get_next_index(ri);
            }
            packed += esz;
        }
        else
        {
            for (b = 0; b < esz; b += 4)  /* filtered out: skip it in the ring */
            {
                ri = log_ram_get_next_index(ri);
            }
        }
        walked += esz;
    }

    *consumed = walked;
    return packed;
}

/**
 * @brief Advance the RAM consume cursor past `consumed` bytes after a pack,
 *        decrement pending_len, bump flush_count and re-stamp the header.
 * @note  Caller must hold the lock and pass the exact byte count reported by the
 *        paired log_ram_pack_ext() so read_index lands on an entry boundary.
 */
void log_ram_consume(U16 consumed)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 ri = header->read_index;
    U16 b;

    for (b = 0; b < consumed; b += 4)
    {
        ri = log_ram_get_next_index(ri);
    }
    header->read_index = ri;

    if (header->pending_len >= consumed)
    {
        header->pending_len -= consumed;
    }
    else
    {
        header->pending_len = 0;
    }
    header->flush_count++;
    header->checksum = LOG_CALC_STRUCT_CHECKSUM(header);
}

WW_BOOL log_ram_is_need_flush(void)
{
    if (log_ext_mem_is_full() == WW_TRUE)
    {
        return WW_FALSE;
    }

    if (g_ram_buffer.header->pending_len >= g_ram_buffer.threshold)
    {
        return WW_TRUE;
    }

    return WW_FALSE;
}

U32 log_ram_get_threshold(void)
{
    return g_ram_buffer.threshold;
}

U32 log_ram_get_flush_count(void)
{
    return g_ram_buffer.header->flush_count;
}

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */

/*************************** global function end *****************************/

#endif /* CONFIG_N_LOG_BACKEND_RAM */
