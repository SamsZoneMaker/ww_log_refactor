/*************************** description start ***************************/
/* ww_log external-storage block ring.
 *
 * Owns the LOG partition on the external device (EEPROM/flash): a ring of
 * fixed-size self-describing LOGH blocks plus a fixed FLOG control footer in
 * the tail slot (geometry in n_ww_log_storage.h). Handles partition discovery,
 * the per-block flush (pack -> CRC -> slot write -> footer) and the RING/FREEZE
 * full policy.
 *
 * The RAM ring lives entirely in n_ww_log_ram.c; the flush path drains it via
 * log_ram_pack_block() / log_ram_consume() rather than touching g_ram_buffer,
 * so the two halves stay decoupled. This whole file compiles to nothing unless
 * the EXT_MEM backend is enabled. */
/*************************** description end *****************************/

// #include <>
// #include ""
#include "ww_std.h"
#include "log/n_ww_log_storage.h"
#include "log/n_ww_log_macro.h"    /* N_RETURN_*_IF_TRUE + (via def) encode accessors */
#include "log/n_ww_log_task.h"

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

#include "init_ex.h"
#include "arch/arch.h"
#include "drivers/flash.h"
#include "drivers/eeprom.h"

/* temp */
#include "FreeRTOS.h"
#include "task.h"

/*************************** static variable start ***************************/
/* to be used only in this file */
static const struct device *g_extmem_dev = NULL;
static LOG_EXT_CTX_T g_log_ext_ctx = {0};
/* footer is built on the fly in ext_footer_write() -- no persistent copy kept */
/*************************** static variable end *****************************/


/*************************** static function start ***************************/
/* to be used only in this file */

/* ---- device-level access (type-dispatched), absolute partition offsets ---- */
static int ext_dev_write(U32 abs_off, U8 *buf, U32 len)
{
    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
    {
        return flash_write(g_extmem_dev, abs_off, buf, len);
    }
    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_EEPROM)
    {
        return eeprom_write(g_extmem_dev, abs_off, buf, len);
    }
    return WW_ERR;
}

static int ext_dev_read(U32 abs_off, U8 *buf, U32 len)
{
    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
    {
        return flash_read(g_extmem_dev, abs_off, buf, len);
    }
    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_EEPROM)
    {
        return eeprom_read(g_extmem_dev, abs_off, buf, len);
    }
    return WW_ERR;
}

/* Erase one block slot before re-writing it.
 * NOTE (real HW): NOR flash erase granularity is a sector (often 4KB), so a 512B
 * block ring can only be overwritten in place on EEPROM (byte-writable). On NOR
 * the whole LOG partition is one sector here; sub-sector slot erase is not
 * possible on real flash -> a flash ring needs sector-aware logic. TODO when the
 * flash backend is exercised on target. EEPROM (the primary path) is a no-op. */
static void ext_dev_erase_block(U32 abs_off)
{
    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
    {
        (void)flash_erase(g_extmem_dev, abs_off, LOG_EXT_BLOCK_SIZE);
    }
}

/* Erase / clear the whole LOG partition at init.
 * BUGFIX: the old init called eeprom_write(dev, off, 0, size) -- data=NULL, which
 * the driver rejects (-EINVAL), so the area was never actually cleared. */
static int ext_partition_erase(void)
{
    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_FLASH)
    {
        return flash_erase(g_extmem_dev, g_log_ext_ctx.log_offset, g_log_ext_ctx.log_size);
    }
    if (g_log_ext_ctx.ext_mem_type == EXT_MEM_EEPROM)
    {
        static U8 clr[256];
        U32 off = g_log_ext_ctx.log_offset;
        U32 rem = g_log_ext_ctx.log_size;
        ww_memset(clr, 0xFF, sizeof(clr));
        while (rem > 0)
        {
            U32 chunk = (rem > sizeof(clr)) ? sizeof(clr) : rem;
            if (eeprom_write(g_extmem_dev, off, clr, chunk) != WW_OK)
            {
                return WW_ERR;
            }
            off += chunk;
            rem -= chunk;
        }
        return WW_OK;
    }
    return WW_ERR;
}

/* Write the ring-control footer to the fixed tail slot [log_size-32, log_size). */
static void ext_footer_write(void)
{
    LOG_EXT_FOOTER_T f;
    U32 foff = g_log_ext_ctx.log_offset + g_log_ext_ctx.log_size - LOG_EXT_FOOTER_SIZE;

    ww_memset(&f, 0, sizeof(f));
    f.magic                = LOG_EXTMEM_MAGIC;
    f.mem_type             = (U16)g_log_ext_ctx.ext_mem_type;
    f.write_slot           = g_log_ext_ctx.write_slot;
    f.wrap_count           = g_log_ext_ctx.wrap_count;
    f.next_seq             = g_log_ext_ctx.next_seq;
    f.log_count            = log_ram_get_log_count();
    f.last_flush_timestamp = ww_cycle_get_32();
    f.checksum             = LOG_CALC_STRUCT_CHECKSUM(&f);

    ext_dev_erase_block(foff);
    (void)ext_dev_write(foff, (U8 *)&f, sizeof(f));
}

/* Try to resume the block ring from the tail footer after a (power-loss) restart.
 * The ext context (g_log_ext_ctx) is RAM-resident and lost on reboot, but the
 * device keeps the previously flushed blocks + footer. If the footer's magic,
 * checksum, device type and write_slot all validate, restore the ring cursor so
 * new flushes continue after the newest block WITHOUT erasing prior boots' logs.
 * Returns WW_TRUE on a successful resume, WW_FALSE on first use / no valid footer
 * (caller then erases and starts fresh).
 * @note Assumes block_count is already computed by the caller. */
static WW_BOOL ext_footer_try_resume(void)
{
    LOG_EXT_FOOTER_T f;
    U32 foff = g_log_ext_ctx.log_offset + g_log_ext_ctx.log_size - LOG_EXT_FOOTER_SIZE;

    if (ext_dev_read(foff, (U8 *)&f, sizeof(f)) != WW_OK)
    {
        return WW_FALSE;
    }
    if (f.magic != LOG_EXTMEM_MAGIC)
    {
        return WW_FALSE;                       /* first use / erased / garbage */
    }
    if (LOG_CALC_STRUCT_CHECKSUM(&f) != f.checksum)
    {
        return WW_FALSE;                       /* torn / corrupt footer        */
    }
    if (f.mem_type != (U16)g_log_ext_ctx.ext_mem_type)
    {
        return WW_FALSE;                       /* footer from a different layout */
    }
    if (f.write_slot >= g_log_ext_ctx.block_count)
    {
        return WW_FALSE;                       /* cursor out of range          */
    }

    g_log_ext_ctx.write_slot       = f.write_slot;
    g_log_ext_ctx.wrap_count       = f.wrap_count;
    g_log_ext_ctx.next_seq         = f.next_seq;
    g_log_ext_ctx.ext_write_offset = g_log_ext_ctx.log_offset
                                   + (U32)f.write_slot * LOG_EXT_BLOCK_SIZE;
    return WW_TRUE;
}

/*************************** static function end *****************************/


/*************************** global function start ***************************/

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

    /* Step 4: Compute the block-ring geometry. */
    g_log_ext_ctx.block_count = (U16)((g_log_ext_ctx.log_size - LOG_EXT_FOOTER_SIZE)
                                      / LOG_EXT_BLOCK_SIZE);

    if (g_log_ext_ctx.block_count == 0)
    {
        /* Partition too small to hold even one block + footer. */
        g_log_ext_ctx.log_part_valid = WW_FALSE;
        g_log_ext_ctx.initialized = WW_TRUE;
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    /* Step 5: Resume from a valid footer (power-loss retained archive) so prior
     * boots' blocks survive the reboot; only erase + start fresh when there is no
     * valid footer (first use / corrupt). This is the device-side half of the
     * "RAM preserved -> reset -> recover -> decode" closed loop. */
    if (ext_footer_try_resume() == WW_TRUE)
    {
        g_log_ext_ctx.initialized = WW_TRUE;
        ww_printf("[LOG][EXT]: Resumed ring (slot=%u wrap=%u next_seq=%u)\n",
                  g_log_ext_ctx.write_slot, g_log_ext_ctx.wrap_count,
                  g_log_ext_ctx.next_seq);
        return LOG_EXT_OK;
    }

    g_log_ext_ctx.write_slot       = 0;
    g_log_ext_ctx.wrap_count       = 0;
    g_log_ext_ctx.next_seq         = 0;
    g_log_ext_ctx.ext_write_offset = g_log_ext_ctx.log_offset;

    if (ext_partition_erase() != WW_OK)
    {
        g_log_ext_ctx.initialized = WW_TRUE;
        return LOG_EXT_ERR_CLEAR_FAIL;
    }

    ext_footer_write();
    g_log_ext_ctx.initialized = WW_TRUE;
    return LOG_EXT_OK;
}

int log_ext_mem_clear(void)
{
    if (g_extmem_dev == NULL || g_log_ext_ctx.log_part_valid == 0)
    {
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    ww_printf("[LOG][EXT]: Clearing log partition ...\n");

    if (ext_partition_erase() != WW_OK)
    {
        return LOG_EXT_ERR_CLEAR_FAIL;
    }

    /* Reset the ring back to slot 0. */
    g_log_ext_ctx.write_slot       = 0;
    g_log_ext_ctx.wrap_count       = 0;
    g_log_ext_ctx.next_seq         = 0;
    g_log_ext_ctx.ext_write_offset = g_log_ext_ctx.log_offset;
    ext_footer_write();

    ww_printf("[LOG][EXT]: Clear done\n");
    return LOG_EXT_OK;
}

/* Drop the RAM-resident ext context so the next log_ext_mem_available() re-runs
 * log_ext_mem_init() -- i.e. simulate a reboot where g_log_ext_ctx is lost but
 * the device bytes (blocks + footer) persist. Used by the resume self-test; on
 * real hardware the reboot zeroes the static for you. */
void log_ext_force_reinit(void)
{
    ww_memset(&g_log_ext_ctx, 0, sizeof(g_log_ext_ctx));
    g_extmem_dev = NULL;
}

/**
 * @brief Flush ONE block of whole entries from the RAM ring to external storage.
 *
 * Packs as many complete entries as fit in a LOGH block payload (never splitting
 * an entry), stamps a per-block CRC, and writes the block to the current ring
 * slot, then updates the tail footer. On RING policy the oldest slot is
 * overwritten on wrap; on FREEZE flushing stops once every slot is filled.
 *
 * Drains one block per call (matches the "~512B per move" cadence). The flush
 * task re-arms while pending_len stays above threshold, so a backlog drains over
 * successive wake-ups.
 *
 * The RAM ring is snapshotted (log_ram_pack_block) and consumed
 * (log_ram_consume) under the log mutex, so concurrent writers always see a
 * consistent ring; the slow device I/O happens after the lock is released.
 *
 * @note NOT re-entrant: uses a static block buffer and fills it under the lock
 *       but writes the device after unlocking, so it must have a single caller
 *       (the flush task). Do not call it concurrently from another context.
 * @return LOG_EXT_OK on success/no-op, or a LOG_EXT_ERR_* code.
 */
int log_ram_flush(void)
{
    static U8 block[LOG_EXT_BLOCK_SIZE];
    LOG_BLOCK_HEADER_T *bh = (LOG_BLOCK_HEADER_T *)block;
    U8 *payload = block + LOG_EXT_BLOCK_HEADER_SIZE;
    U16 packed, ecount;
    U32 slot_off;
    int ret;

    if (log_ext_mem_available() == WW_FALSE)
    {
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    log_mutex_lock_wait();

    if (get_current_usage() == 0)
    {
        log_mutex_unlock();
        return LOG_EXT_OK;
    }

#ifdef CONFIG_N_LOG_EXT_POLICY_FREEZE
    if (log_ext_mem_is_full() == WW_TRUE)
    {
        log_mutex_unlock();
        return LOG_EXT_ERR_PT_FULL;   /* archive frozen: keep earliest logs */
    }
#endif

    /* Snapshot one block of whole entries, then advance the RAM consume cursor
     * (all under the lock so writers see a consistent ring). */
    ww_memset(block, 0xFF, sizeof(block));   /* 0xFF padding == erased/end marker */
    packed = log_ram_pack_block(payload, &ecount);
    if (packed == 0)
    {
        log_mutex_unlock();
        return LOG_EXT_OK;
    }

    log_ram_consume(packed);

    bh->magic       = LOG_BLOCK_MAGIC;
    bh->seq         = g_log_ext_ctx.next_seq;
    bh->timestamp   = ww_cycle_get_32();
    bh->data_size   = packed;
    bh->entry_count = ecount;
    bh->crc         = log_calc_checksum(payload, packed); /* payload is 4-aligned */
    bh->reserved1   = 0;
    bh->reserved2   = 0;

    slot_off = g_log_ext_ctx.log_offset
             + (U32)g_log_ext_ctx.write_slot * LOG_EXT_BLOCK_SIZE;

    /* Advance ring position (next slot, wrap -> overwrite oldest). */
    g_log_ext_ctx.next_seq++;
    g_log_ext_ctx.write_slot++;
    if (g_log_ext_ctx.write_slot >= g_log_ext_ctx.block_count)
    {
        g_log_ext_ctx.write_slot = 0;
        g_log_ext_ctx.wrap_count++;
    }
    g_log_ext_ctx.ext_write_offset = g_log_ext_ctx.log_offset
             + (U32)g_log_ext_ctx.write_slot * LOG_EXT_BLOCK_SIZE;

    log_mutex_unlock();

    /* Slow device I/O outside the lock. Write only header+payload (the slot tail
     * keeps whatever was there; the decoder uses data_size and a fixed slot
     * stride, so stale tail bytes are skipped). */
    ext_dev_erase_block(slot_off);
    ret = ext_dev_write(slot_off, block, LOG_EXT_BLOCK_HEADER_SIZE + packed);
    ext_footer_write();

    return (ret == WW_OK) ? LOG_EXT_OK : LOG_EXT_ERR_WRITE_FAIL;
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

U16 log_ext_get_block_count(void)
{
    return g_log_ext_ctx.block_count;
}

U16 log_ext_get_write_slot(void)
{
    return g_log_ext_ctx.write_slot;
}

U32 log_ext_get_wrap_count(void)
{
    return g_log_ext_ctx.wrap_count;
}

U32 log_ext_get_next_seq(void)
{
    return g_log_ext_ctx.next_seq;
}

WW_BOOL log_ext_mem_is_full(void)
{
    /* FREEZE: "full" once every slot has been written once (wrapped) -> stop
     * flushing to preserve the earliest logs.
     * RING: never full -- the oldest slot is overwritten on wrap. */
#ifdef CONFIG_N_LOG_EXT_POLICY_FREEZE
    return (g_log_ext_ctx.wrap_count >= 1) ? WW_TRUE : WW_FALSE;
#else
    return WW_FALSE;
#endif
}

U32 log_ext_mem_get_used(void)
{
    /* Bytes of block storage in use (capped at the block area when wrapped). */
    U32 block_area = (U32)g_log_ext_ctx.block_count * LOG_EXT_BLOCK_SIZE;
    if (g_log_ext_ctx.wrap_count > 0)
    {
        return block_area;
    }
    return (U32)g_log_ext_ctx.write_slot * LOG_EXT_BLOCK_SIZE;
}

U32 log_ext_mem_get_remaining(void)
{
    U32 block_area = (U32)g_log_ext_ctx.block_count * LOG_EXT_BLOCK_SIZE;
    return block_area - log_ext_mem_get_used();
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

/*************************** global function end *****************************/

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */
