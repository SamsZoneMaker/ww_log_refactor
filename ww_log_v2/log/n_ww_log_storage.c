/*************************** description start ***************************/
/* to add description for this file if needed */
/*************************** description end *****************************/

// #include <>
// #include ""
#include "ww_std.h"
#include "log/n_ww_log_storage.h"
#include "log/n_ww_log_control.h"
#include "log/n_ww_log_task.h"

#include "init_ex.h"
#include "arch/arch.h"
#include "drivers/flash.h"
#include "drivers/eeprom.h"

/* temp */
#include "FreeRTOS.h"
#include "task.h"

/*************************** global variable start ***************************/
/* to be used in all files */
/*************************** global variable end *****************************/

/*************************** macro definition start ***************************/
/* to be used only in this file */
/*************************** macro definition end *****************************/

/*************************** type definition start ***************************/
/* to be used only in this file */
/*************************** type definition end *****************************/

/*************************** declaration start ***************************/
/* to be used only in this file */
/*************************** declaration end *****************************/

/*************************** static variable start ***************************/
/* to be used only in this file */
static LOG_RAM_BUFFER_T g_ram_buffer = {0};

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
static const struct device *g_extmem_dev = NULL;
static LOG_EXT_CTX_T g_log_ext_ctx = {0};
static LOG_EXT_FOOTER_T g_log_ext_footer = {0};
#endif
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

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
static void init_ext_footer(LOG_EXT_FOOTER_T *footer)
{
    ww_memset(footer, 0xff, sizeof(LOG_EXT_FOOTER_T));

    footer->magic = LOG_EXTMEM_MAGIC;
    footer->mem_type = g_log_ext_ctx.ext_mem_type;
    footer->full_tags = 0;
    footer->init_timestamp = ww_cycle_get_32();
    footer->last_flush_timestamp = 0;
    footer->log_count = 0;
    footer->reserved1 = 0;
    footer->reserved2 = 0;

    footer->checksum = LOG_CALC_STRUCT_CHECKSUM(footer);
}

static int log_ext_mem_write(U8 *data, U32 len)
{
    int ret;

    /* Step 0: Make sure already initialized for using */
    if (g_log_ext_ctx.initialized != WW_TRUE)
    {
        int ret = log_ext_mem_init();
        if (ret != LOG_EXT_OK && ret != LOG_EXT_ERR_NO_EXT_MEM)
        {
            return ret;
        }
    }

    if (g_extmem_dev == NULL || g_log_ext_ctx.log_part_valid != WW_TRUE)
    {
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    if (g_log_ext_ctx.ext_write_offset + len > g_log_ext_ctx.log_size)
    {
        // ww_printf("[LOG][EXT]: Partition full\n");
        return LOG_EXT_ERR_PT_FULL;
    }

    U32 write_offset = g_log_ext_ctx.log_offset + g_log_ext_ctx.ext_write_offset;
    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
    {
        ww_printf("[LOG][EXT]: Writing %u bytes to Flash at 0x%X\n",
            len, write_offset);
        ret = flash_write(g_extmem_dev, write_offset, data, len);
    }
    else if (g_log_ext_ctx.ext_mem_type == EXT_MEM_EEPROM)
    {
        ww_printf("[LOG][EXT]: Writing %u bytes to EEPROM at 0x%X\n", len, write_offset);
        ret = eeprom_write(g_extmem_dev, write_offset, data, len);
    }
    else
    {
        return LOG_EXT_ERR_WRITE_FAIL;
    }

    if (ret == WW_OK)
    {
        g_log_ext_ctx.ext_write_offset += len;

        /* Todo: Debugging */
        if (g_log_ext_ctx.ext_write_offset > g_log_ext_ctx.log_size)
        {
            // ww_printf("[LOG][EXT]: Error!! Over the log area!\n");
        }
    }

    return (ret == 0) ? LOG_EXT_OK : LOG_EXT_ERR_WRITE_FAIL;
}
#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */

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
        rtn = validate_header(g_ram_buffer.header);
    }

    // if (rtn)
    // {
    // }
    if (rtn != WW_OK || force_clear == WW_ENABLE)
    {
        init_header(g_ram_buffer.header);
        ww_memset(g_ram_buffer.data, 0, g_ram_buffer.data_size);
    }
    ww_printf("[LOG][RAM]: Initialized (force_clear=%u)\n", force_clear); // Todo: set as comment

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

    if (log_mutex_lock() != WW_OK)
    {
        return LOG_EXT_ERR_MUTEX_FAIL;
    }

    U16 write_idx = header->write_index;
    U16 required = 4 + param_count * 4; /* 4 bytes for encoded + 4*N for params */
    U16 available = log_ram_get_available(); /* Check if have enough space (considering wrap-around) */

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
    if (log_ram_is_need_flush() == WW_TRUE)
    {
        log_flush_notify();
    }
#endif

    if (required > available)
    {
        if (required > g_ram_buffer.data_size)
        {
            // ww_printf("[LOG][EXT]: Single log too large (%u bytes), discarded\n", required);
            log_mutex_unlock();
            return WW_OK;
        }

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
        if (log_ext_mem_is_full() == WW_TRUE)
        {
            log_flush_notify();
        }
#endif

        U16 wrap_bytes = required - available;

        write_idx = 0;

        if (header->read_index < wrap_bytes)
        {
            U32 lost_bytes = header->read_index;
            header->read_index = wrap_bytes;


#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
            if (header->pending_len >= lost_bytes)
            {
                header->pending_len -= lost_bytes;
            }
            else
            {
                header->pending_len = 0;
            }
#endif

            ww_printf("[LOG][RAM]: Buffer wrap, lost %u bytes of old data\n", lost_bytes);
        }
    }

    /* Write encoded LOG entry */
    // ww_printf("Log Base Addr : %p\n", (void*)DLM_MAINTAIN_LOG_BASE_ADDR);
    // ww_printf("Data Start Addr: %p\n", (void*)g_ram_buffer.data);

    *(U32 *)(g_ram_buffer.data + write_idx) = encoded;
    write_idx = log_ram_get_next_index(write_idx);
    header->log_count += 1;

    /* Write parameters */
    for (U8 i = 0; i < param_count; i++)
    {
        *(U32 *)(g_ram_buffer.data + write_idx) = params[i];
        write_idx = log_ram_get_next_index(write_idx);
    }

    header->write_index = write_idx;

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
    header->pending_len += required;
#endif

    /* Update checksum */
    header->checksum = LOG_CALC_STRUCT_CHECKSUM(header);

    log_mutex_unlock();

    return WW_OK;
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
    ww_printf("sizeof(LOG_RAM_HEADER_T) = %u\n", sizeof(LOG_RAM_HEADER_T));
    ww_printf("LOG_RAM_HEADER_SIZE = %u\n", LOG_RAM_HEADER_SIZE);
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


#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

/**
 * @brief Initialize the external storage log module
 * @note detect external storage type, verify partition table, gte LOG pt_info
 * @return LOG_EXT_OK = success, special value -> different errors
 */
WW_RTN log_ext_mem_init(void)
{
    REG_WW_STUS_SYS_INFO_U *sys_info;
    PART_TABLE_T *pt;
    PART_ENTRY_T *log_entry;

    if (g_log_ext_ctx.initialized == WW_TRUE)
    {
        return LOG_EXT_OK;
    }

    ww_printf("[LOG][EXT]: Initializing external memory ... \n");

    /* Step 1: Get the type of external memory */
    sys_info = reg_ww_stus_acc_sys_info_get();
    // ww_printf(" bootMode   = %u\n", sys_info->sub.bootMode);
    // ww_printf(" extMemType = %u\n", sys_info->sub.extMemType);
    g_log_ext_ctx.ext_mem_type = sys_info->sub.extMemType;

    // ww_printf("[LOG][EXT]: ext_mem_type = %u\n", g_log_ext_ctx.ext_mem_type);

    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
    {
        g_extmem_dev = ww_get_device("SPINOR_WM");
    }
    else if (g_log_ext_ctx.ext_mem_type == EXT_MEM_EEPROM)
    {
        g_extmem_dev = ww_get_device("EEPROM_1");
    }
    else
    {
        g_log_ext_ctx.log_part_valid = WW_FALSE;
        g_log_ext_ctx.initialized = WW_TRUE;
        g_log_ext_ctx.ext_write_offset = 0;
        if (g_log_ext_ctx.ext_mem_type == EXT_MEM_NONE)
        {
            /* if no ext_mem, but not fatal */
            // ww_printf("[LOG][EXT]: No external memory\n");
            return LOG_EXT_ERR_NO_EXT_MEM;
        }
        else
        {
            // ww_printf("[LOG][EXT]: Get external memory Error\n");
            return LOG_EXT_ERR;
        }
    }

    /* Step 2: Read and verify partition table */
    pt = pt_info_read();
    if (pt == NULL)
    {
        g_log_ext_ctx.log_part_valid = WW_FALSE;
        g_log_ext_ctx.initialized = WW_TRUE;
        // ww_printf("[LOG][EXT]: Partition table is NULL\n");
        return LOG_EXT_ERR_PT_NULL;
    }

    /* Todo: For debugging */
    // ww_printf("====== Partition Table ======\n");
    // ww_printf("magic:      0x%08X\n", pt->magic);
    // ww_printf("version:    0x%08X\n", pt->version);
    // ww_printf("product:    0x%08X\n", pt->product);
    // ww_printf("ptableSize: %u\n", pt->ptableSize);
    // ww_printf("pentryNum:  %u\n", pt->pentryNum);

    // for (U16 i = 0; i < pt->pentryNum && i < 16; i++)
    // {
    //     PART_ENTRY_T *temp_pe = &pt->pentry[i];
    //     ww_printf("[%u] type=%u, od=%u, slot=%u, offset=0x%08X, size=0x%08X\n",
    //               i, temp_pe->part_type, temp_pe->part_id, temp_pe->slot_id,
    //               temp_pe->part_offset, temp_pe->part_size);
    // }
    // ww_printf("=============================\n");

    if (pt_table_check_valid(pt) != WW_OK)
    {
        g_log_ext_ctx.log_part_valid = WW_FALSE;
        g_log_ext_ctx.initialized = WW_TRUE;
        // ww_printf("[LOG][EXT]: Partition table invalid\n");
        return LOG_EXT_ERR_PT_INVALID;
    }

    /* Step 3: Get Log info in pt_table */
    log_entry = pt_entry_get_by_key(pt, PART_ENTRY_TYPE_LOG, 0, 0);
    if (log_entry == NULL)
    {
        /* Example: 16K EEPROM bin */
        g_log_ext_ctx.log_part_valid = WW_FALSE;
        g_log_ext_ctx.initialized = WW_TRUE;
        // ww_printf("[LOG]: Get log pt_table address failed \n");
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    /* Save pt info for use */
    g_log_ext_ctx.log_offset = log_entry->part_offset;
    g_log_ext_ctx.log_size = log_entry->part_size;
    g_log_ext_ctx.log_part_valid = WW_TRUE;

    /* Step 4: Init log area of extmem */
    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
    {
        flash_erase(g_extmem_dev, g_log_ext_ctx.log_offset, g_log_ext_ctx.log_size);
        g_log_ext_ctx.ext_write_offset = 0;
        // ww_printf("[LOG][EXT]: Erasing Flash ... \n");
    }
    else if (g_log_ext_ctx.ext_mem_type == EXT_MEM_EEPROM)
    {
        eeprom_write(g_extmem_dev, g_log_ext_ctx.log_offset, 0, g_log_ext_ctx.log_size);
        g_log_ext_ctx.ext_write_offset = 0;
        // ww_printf("[LOG][EXT]: Initializing eeprom log area ...\n");
    }
    else
    {
        return LOG_EXT_ERR_NO_EXT_MEM;
    }

    init_ext_footer(&g_log_ext_footer);
    g_log_ext_ctx.initialized = WW_TRUE;
    return LOG_EXT_OK;
}

int log_ext_mem_clear(void)
{
    int ret = 0;

    if (g_extmem_dev == NULL || g_log_ext_ctx.log_part_valid == 0)
    {
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    ww_printf("[LOG][EXT]: Clearing log partition ...\n");

    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
    {
        ret = flash_erase(g_extmem_dev, g_log_ext_ctx.log_offset, g_log_ext_ctx.log_size);
    }
    else if (g_log_ext_ctx.ext_mem_type == EXT_MEM_EEPROM)
    {
        static U8 clear_buf[256];
        ww_memset(clear_buf, 0xFF, sizeof(clear_buf));

        U32 offset = g_log_ext_ctx.log_offset;
        U32 remaining = g_log_ext_ctx.log_size;

        while (remaining > 0)
        {
            U32 chunk = (remaining > sizeof(clear_buf)) ? sizeof(clear_buf) : remaining;
            ret = eeprom_write(g_extmem_dev, offset, clear_buf, chunk);
            if (ret != 0) break;
            offset += chunk;
            remaining -= chunk;
        }
    }

    if (ret == WW_OK)
    {
        g_log_ext_ctx.ext_write_offset = 0;
        ww_printf("[LOG][EXT]: Clear done\n");

        return LOG_EXT_OK;
    }
    else
    {
        return LOG_EXT_ERR_CLEAR_FAIL;
    }
}

int log_ram_flush(void)
{
    int ret;
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 read_idx;
    U32 flush_len;
    static U8 temp_buf[1024];

    log_mutex_lock_wait();

    if (header->pending_len == 0)
    {
        log_mutex_unlock();
        return LOG_EXT_OK;
    }

    ww_printf("[LOG_RAM]: Flush triggered, pending_len = %u, write_index = %u\n",
              header->pending_len, header->write_index);

    if (log_ext_mem_available() == WW_FALSE)
    {
        ww_printf("[LOG_RAM]: External storage not available, clearing RAM\n");
        log_ram_index_reset();
        log_mutex_unlock();
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    if (log_ext_mem_is_full() == WW_TRUE || g_log_ext_footer.full_tags == WW_TRUE)
    {
        ww_printf("[LOG_RAM]: External storage full, skip flush\n");
        log_mutex_unlock();
        return LOG_EXT_ERR_PT_FULL;
    }

    read_idx = header->read_index;
    flush_len = header->pending_len;

    /* Restriction 1: Data flush cannot exceed the remaining external storage space. */
    U32 ext_remaining = g_log_ext_ctx.log_size - g_log_ext_ctx.ext_write_offset;
    ww_printf("[LOG][EXT]: Truncated flush to %u (ext remaining)\n", ext_remaining);
    if (flush_len > ext_remaining)
    {
        flush_len = ext_remaining;
        ww_printf("[LOG_RAM]: Truncate flush to %u (ext remaining)\n", flush_len);
    }

    /* Limitation 2: Flush cannot exceed the size of the temporary buffer zone. */
    if (flush_len > sizeof(temp_buf))
    {
        flush_len = sizeof(temp_buf);
    }

    /* Restriction 3: Cannot extend beyond the end of the buffer (to avoid wrap-around processing) */
    if (read_idx + flush_len > g_ram_buffer.data_size)
    {
        flush_len = g_ram_buffer.data_size - read_idx;
    }

    ww_printf("[LOG_RAM]: Flushing (%u, %u), len=%u\n",
              read_idx, read_idx + flush_len, flush_len);

    ww_memcpy(temp_buf, g_ram_buffer.data + read_idx, flush_len);

    /* Update RAM index */
    header->read_index = read_idx + flush_len;
    if (header->read_index >= g_ram_buffer.data_size)
    {
        header->read_index = 0;
    }
    header->pending_len -= flush_len;

    g_log_ext_footer.log_count = header->log_count;

    log_mutex_unlock();

    ret = log_ext_mem_write(temp_buf, flush_len);

    if (ret == LOG_EXT_OK)
    {
        header->flush_count++;
        g_log_ext_footer.last_flush_timestamp = ww_cycle_get_32();
        ww_printf("[LOG_RAM]: Flush OK, wrote %u bytes, flush_count = %u\n",
                  flush_len, header->flush_count);
    }
    else
    {
        ww_printf("[LOG_RAM]: Flush FAILED, ret=%d, expected: %d \n", ret, LOG_EXT_ERR_NO_LOG_PART);
        log_mutex_lock_wait();
        header->read_index = read_idx;
        header->pending_len += flush_len;
        log_mutex_unlock();
    }

    if (log_ram_is_need_flush() == WW_TRUE)
    {
        log_flush_notify();
    }

    if (log_ext_mem_is_full() == WW_TRUE)
    {
        if (g_log_ext_footer.full_tags == WW_FALSE)
        {
            g_log_ext_footer.full_tags = WW_TRUE;
            ww_memcpy(temp_buf, &g_log_ext_footer, sizeof(LOG_EXT_FOOTER_T));
            ret = log_ext_mem_write(temp_buf, sizeof(LOG_EXT_FOOTER_T));
            if (ret == 0)
            {
                // ww_printf("[LOG][EXT]: Filled footer success.");
                return LOG_EXT_OK;
            }
            return LOG_EXT_ERR_PT_FULL;
        }
    }

    return ret;
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

U32 log_ext_get_log_size(void)
{
    return g_log_ext_ctx.log_size;
}

U8 log_ext_get_mem_type(void)
{
    return g_log_ext_ctx.ext_mem_type;
}

U8 log_ext_get_initialized(void)
{
    return g_log_ext_ctx.initialized;
}

U8 log_ext_get_part_valid(void)
{
    return g_log_ext_ctx.log_part_valid;
}

U32 log_ext_get_log_offset(void)
{
    return g_log_ext_ctx.log_offset;
}

U32 log_ext_get_write_offset(void)
{
    return g_log_ext_ctx.ext_write_offset;
}

WW_BOOL log_ext_mem_is_full(void)
{
    if (g_log_ext_ctx.ext_write_offset >= (g_log_ext_ctx.log_size - sizeof(LOG_EXT_FOOTER_T)))
    {
        return WW_TRUE;
    }

    return WW_FALSE;
}

U32 log_ext_mem_get_used(void)
{
    return g_log_ext_ctx.ext_write_offset;
}

U32 log_ext_mem_get_remaining(void)
{
    return g_log_ext_ctx.log_size - g_log_ext_ctx.ext_write_offset;
}

U32 log_ram_get_flush_count(void)
{
    return g_ram_buffer.header->flush_count;
}

WW_BOOL log_ext_mem_available(void)
{
    if (g_log_ext_ctx.initialized == WW_FALSE)
    {
        log_ext_mem_init();
    }

    return g_log_ext_ctx.log_part_valid;
}

int log_ext_mem_read(U8 *buf, U32 len)
{
    int ret = 0;

    if (g_extmem_dev == NULL || g_log_ext_ctx.log_part_valid == 0)
    {
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    if (len > g_log_ext_ctx.log_size)
    {
        len = g_log_ext_ctx.log_size;
    }

    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
    {
        ret = flash_read(g_extmem_dev, g_log_ext_ctx.log_offset, buf, len);
    }
    else if (g_log_ext_ctx.ext_mem_type == EXT_MEM_EEPROM)
    {
        ret = eeprom_read(g_extmem_dev, g_log_ext_ctx.log_offset, buf, len);
    }

    if (ret == WW_OK)
    {
        return LOG_EXT_OK;
    }
    else
    {
        return LOG_EXT_ERR_READ_FAIL;
    }
}

void log_ext_mem_dump(void)
{
    static U8 buf[1024] = {0};
    U32 offset = 0;

    ww_printf("\n========= External Memory LOG Dump =========\n");
    // ww_printf("Used: %u bytes\n",
    //           g_log_ext_ctx.ext_write_offset, g_log_ext_ctx.log_size);
    ww_printf("Log part offset is 0x%X\n", g_log_ext_ctx.log_offset);
    ww_printf("Log offset is 0x%X\n", g_log_ext_ctx.log_offset + offset);

    // while (offset < g_log_ext_ctx.log_size)
    while (offset < 0x100)
    {
        U32 chunk = 16;
        if (offset + chunk > g_log_ext_ctx.log_size) {
            chunk = g_log_ext_ctx.log_size - offset;
        }

        if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
        {
            flash_read(g_extmem_dev, g_log_ext_ctx.log_offset + offset, buf, chunk);
        }
        else if (g_log_ext_ctx.ext_mem_type == EXT_MEM_EEPROM)
        {
            eeprom_read(g_extmem_dev, g_log_ext_ctx.log_offset + offset, buf, chunk);
        }

        ww_printf("%04X: ", offset);
        for (U32 i = 0; i < chunk; i++)
        {
            ww_printf("%02X ", buf[i]);
        }
        ww_printf("\n");
        vTaskDelay(pdMS_TO_TICKS(10));

        offset += chunk;
    }

    ww_printf("========= Dump End =========\n");
}

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */

/*************************** global function end *****************************/
