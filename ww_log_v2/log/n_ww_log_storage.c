/*************************** description start ***************************/
/* ww_log external-storage append log (log-structured).
 *
 * Owns the LOG partition on the external device (EEPROM/flash): an 8B 'XLOG'
 * partition header written once, followed by an append-only stream of whole
 * encoded entries (geometry in n_ww_log_storage.h). Handles partition discovery,
 * the append flush (level-filter -> append at write_off) and the FREEZE/ERASE
 * full policy. No per-slot erase (impossible on NOR sub-sector) and no per-flush
 * footer -- write_off is RAM-resident and rebuilt on cold boot by scanning.
 *
 * The RAM ring lives entirely in n_ww_log_ram.c; the flush path drains it via
 * log_ram_pack_ext() / log_ram_consume() rather than touching g_ram_buffer,
 * so the two halves stay decoupled. This whole file compiles to nothing unless
 * the EXT_MEM backend is enabled. */
/*************************** description end *****************************/

// #include <>
// #include ""
#include "ww_std.h"
#include "log/n_ww_log_storage.h"
#include "log/n_ww_log_macro.h"    /* N_RETURN_*_IF_TRUE + (via def) encode accessors */
#include "log/n_ww_log_task.h"
#include "log/n_ww_log_api.h"     /* n_ww_log_write_boot_record */

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

#include "init_ex.h"
#include "arch/arch.h"
#include "drivers/flash.h"
#include "drivers/eeprom.h"

/* Flush-marker timestamps and debug-dump pacing. */
#include "FreeRTOS.h"
#include "task.h"

/*************************** static variable start ***************************/
/* to be used only in this file */
static const struct device *g_extmem_dev = NULL;
static LOG_EXT_CTX_T g_log_ext_ctx = {0};

#ifdef CONFIG_N_LOG_EXT_FLUSH_MARKER
/* Set by log_ext_flush_marker_arm() at each flush-task wake; the first
 * data-bearing log_ram_flush() of that drain consumes it by prepending one
 * timestamped marker, then clears it so the rest of the drain stays unmarked. */
static U8 s_flush_marker_due = 0;
#endif
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

/* Length of the longest whole-entry prefix of `buf` that fits in `limit`.
 * Entries are self-describing (4 + pcnt*4), and control records are ordinary
 * entries, so this walks the staged batch uniformly. Used only when the
 * partition is about to fill: the append stream must never end mid-entry, or
 * the cold-boot scan and the host decoder would both mis-parse the tail. */
static U16 ext_whole_entry_prefix(const U8 *buf, U16 len, U16 limit)
{
    U16 off = 0;

    while (off + 4 <= len)
    {
        U32 hdr = *(const U32 *)(buf + off);
        U16 esz = (U16)(4 + (U16)N_WW_LOG_PCNT_OF(hdr) * 4);

        if (off + esz > len || off + esz > limit)
        {
            break;
        }
        off += esz;
    }
    return off;
}

/* Write the 8B partition header at the partition base. Called once when the
 * partition is (re)initialized fresh; never rewritten during normal flushing. */
static int ext_parthdr_write(void)
{
    LOG_EXT_PART_HDR_T h;

    ww_memset(&h, 0, sizeof(h));
    h.magic    = LOG_EXTMEM_MAGIC;
    h.version  = LOG_EXT_FORMAT_VERSION;
    h.reserved = 0;

    return ext_dev_write(g_log_ext_ctx.log_offset, (U8 *)&h, sizeof(h));
}

/* Rebuild write_off after a (cold) restart by scanning the append stream.
 * g_log_ext_ctx is RAM-resident and lost on a cold boot, but the device keeps
 * the partition header + entries. If the header magic/version validate, walk
 * whole entries from just past the header (each entry's pcnt gives its length)
 * until the first 0xFFFFFFFF word (erased tail) or the partition end; write_off
 * lands on that boundary so new flushes append after prior boots' logs WITHOUT
 * erasing them. Returns WW_TRUE on a successful resume, WW_FALSE on first use /
 * no valid header (caller then erases + writes a fresh header).
 * @note A hot restart keeps the noinit ctx and never calls this. */
static WW_BOOL ext_scan_write_off(void)
{
    LOG_EXT_PART_HDR_T h;
    U32 base = g_log_ext_ctx.log_offset;
    U32 end  = g_log_ext_ctx.log_offset + g_log_ext_ctx.log_size;
    U32 off  = base + LOG_EXT_PART_HDR_SIZE;

    if (ext_dev_read(base, (U8 *)&h, sizeof(h)) != WW_OK)
    {
        return WW_FALSE;
    }
    if (h.magic != LOG_EXTMEM_MAGIC || h.version != LOG_EXT_FORMAT_VERSION)
    {
        return WW_FALSE;                       /* first use / erased / garbage */
    }

    /* Walk entries: read a 4B header, stop at the erased marker, else skip
     * 4 + pcnt*4 bytes. Bounds-check every step so a corrupt pcnt cannot run off
     * the partition. */
    while (off + 4 <= end)
    {
        U32 hdr;
        U32 esz;

        if (ext_dev_read(off, (U8 *)&hdr, 4) != WW_OK)
        {
            return WW_FALSE;
        }
        if (hdr == LOG_EXT_ERASED_WORD)
        {
            break;                             /* erased tail -> end of stream */
        }
        esz = 4 + (U32)N_WW_LOG_PCNT_OF(hdr) * 4;
        if (off + esz > end)
        {
            break;                             /* truncated tail -> stop here  */
        }
        off += esz;
    }

    g_log_ext_ctx.write_off = off;
    g_log_ext_ctx.full = (off + 4 > end) ? WW_TRUE : WW_FALSE;
#if CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_FREEZE
    if (g_log_ext_ctx.full == WW_TRUE)
    {
        log_ram_set_ext_full();
    }
    else
    {
        log_ram_clear_ext_full();
    }
#endif
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
    g_log_ext_ctx.ext_mem_type = sys_info->sub.extMemType;

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
        g_log_ext_ctx.write_off = 0;
        if (g_log_ext_ctx.ext_mem_type == EXT_MEM_NONE)
        {
            /* if no ext_mem, but not fatal */
            return LOG_EXT_ERR_NO_EXT_MEM;
        }
        else
        {
            return LOG_EXT_ERR;
        }
    }

    /* Step 2: Read and verify partition table */
    pt = pt_info_read();
    if (pt == NULL)
    {
        g_log_ext_ctx.log_part_valid = WW_FALSE;
        g_log_ext_ctx.initialized = WW_TRUE;
        return LOG_EXT_ERR_PT_NULL;
    }

    if (pt_table_check_valid(pt) != WW_OK)
    {
        g_log_ext_ctx.log_part_valid = WW_FALSE;
        g_log_ext_ctx.initialized = WW_TRUE;
        return LOG_EXT_ERR_PT_INVALID;
    }

    /* Step 3: Get Log info in pt_table */
    log_entry = pt_entry_get_by_key(pt, PART_ENTRY_TYPE_LOG, 0, 0);
    if (log_entry == NULL)
    {
        /* Example: 16K EEPROM bin */
        g_log_ext_ctx.log_part_valid = WW_FALSE;
        g_log_ext_ctx.initialized = WW_TRUE;
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    /* Save pt info for use */
    g_log_ext_ctx.log_offset = log_entry->part_offset;
    g_log_ext_ctx.log_size = log_entry->part_size;
    g_log_ext_ctx.log_part_valid = WW_TRUE;

    /* Step 4: after ERASE the largest possible staged batch must fit in one
     * empty partition. This also covers boot/marker/max-entry requirements. */
    if (g_log_ext_ctx.log_size <
            LOG_EXT_PART_HDR_SIZE + LOG_EXT_FLUSH_STAGE_SIZE)
    {
        g_log_ext_ctx.log_part_valid = WW_FALSE;
        g_log_ext_ctx.initialized = WW_TRUE;
        return LOG_EXT_ERR_NO_LOG_PART;
    }

    /* Step 5: Resume the append stream (power-loss retained archive) by scanning
     * from a valid partition header, so prior boots' entries survive the reboot;
     * only erase + write a fresh header when there is no valid header (first use /
     * corrupt). Device-side half of the "RAM preserved -> reset -> recover ->
     * decode" closed loop. */
    if (ext_scan_write_off() == WW_TRUE)
    {
        g_log_ext_ctx.initialized = WW_TRUE;
        ww_printf("[LOG][EXT]: Resumed append stream (write_off=0x%X full=%u)\n",
                  g_log_ext_ctx.write_off, g_log_ext_ctx.full);
        return LOG_EXT_OK;
    }

    if (ext_partition_erase() != WW_OK)
    {
        g_log_ext_ctx.initialized = WW_TRUE;
        return LOG_EXT_ERR_CLEAR_FAIL;
    }
    if (ext_parthdr_write() != WW_OK)
    {
        g_log_ext_ctx.initialized = WW_TRUE;
        return LOG_EXT_ERR_WRITE_FAIL;
    }
    g_log_ext_ctx.write_off = g_log_ext_ctx.log_offset + LOG_EXT_PART_HDR_SIZE;
    g_log_ext_ctx.full = WW_FALSE;
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
    log_mutex_lock_wait();

    if (ext_partition_erase() != WW_OK)
    {
        log_mutex_unlock();
        return LOG_EXT_ERR_CLEAR_FAIL;
    }

    /* Rewrite the header and reset the append cursor to just past it. */
    if (ext_parthdr_write() != WW_OK)
    {
        log_mutex_unlock();
        return LOG_EXT_ERR_WRITE_FAIL;
    }
    g_log_ext_ctx.write_off = g_log_ext_ctx.log_offset + LOG_EXT_PART_HDR_SIZE;
    g_log_ext_ctx.full = WW_FALSE;
    log_ram_clear_ext_full();
    log_mutex_unlock();

    ww_printf("[LOG][EXT]: Clear done\n");
    /* No boot record needed here: the flush path stamps one ahead of the first
     * batch an empty archive receives, so the identity is re-established the
     * moment there is anything to attribute -- and only then. */
    return LOG_EXT_OK;
}

/* Drop the RAM-resident ext context so the next log_ext_mem_available() re-runs
 * log_ext_mem_init() -- i.e. simulate a reboot where g_log_ext_ctx is lost but
 * the device bytes (header + entries) persist. Used by the resume self-test; on
 * real hardware the reboot zeroes the static for you. */
void log_ext_force_reinit(void)
{
    ww_memset(&g_log_ext_ctx, 0, sizeof(g_log_ext_ctx));
    g_extmem_dev = NULL;
}

/**
 * @brief Flush a batch of whole entries from the RAM ring to external storage.
 *
 * Walks up to LOG_EXT_FLUSH_STAGE_SIZE RAM bytes of whole entries; entries whose
 * level passes CONFIG_N_LOG_EXT_LEVEL_THRESHOLD are copied into a staging buffer and
 * appended at write_off, the rest are just skipped. Every walked entry (persisted
 * or filtered) is consumed from the RAM ring so it does not stall the cursor.
 *
 * On a partition-full condition the policy decides: FREEZE stops appending (the
 * earliest persisted logs are preserved, the newest are dropped); ERASE wipes the
 * partition, rewrites the header, and restarts the stream (the newest are kept).
 *
 * The RAM ring is walked (log_ram_pack_ext) and consumed (log_ram_consume) under
 * the log mutex, and write_off is reserved under it too, so concurrent writers
 * see a consistent ring; the slow device I/O happens after the lock is released.
 *
 * @note NOT re-entrant: uses a static staging buffer and reserves write_off under
 *       the lock but writes the device after unlocking, so it must have a single
 *       caller (the flush task). Do not call it concurrently.
 * @return LOG_EXT_OK on success/no-op, or a LOG_EXT_ERR_* code.
 */
int log_ram_flush(void)
{
    /* A U32 backing array guarantees the alignment required by the encoded
     * stream's word accesses; header checks guarantee exact sizing. */
    static U32 stage_words[LOG_EXT_FLUSH_STAGE_SIZE / sizeof(U32)];
    U8 *stage = (U8 *)stage_words;
    U16 packed, consumed;
    U16 lead = 0;                 /* control-record bytes ahead of `packed` */
    U16 boot_lead = 0;            /* boot record, when the archive is empty   */
    U16 total;                    /* control records + packed device bytes */
    U32 dst_off = 0, end, avail;
    int archive_empty;
    int need_erase = 0;
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

#if CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_FREEZE
    if (g_log_ext_ctx.full == WW_TRUE)
    {
        log_mutex_unlock();
        return LOG_EXT_ERR_PT_FULL;   /* FREEZE: archive frozen, keep earliest */
    }
#endif

#if CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE
    archive_empty = (g_log_ext_ctx.write_off ==
                     g_log_ext_ctx.log_offset + LOG_EXT_PART_HDR_SIZE);

    /* An archive holding entries but no boot record cannot be attributed to a
     * map, so stamp one ahead of the first batch a fresh archive receives.
     * Doing it here rather than at init/clear time makes it an invariant of the
     * writer: whatever emptied the archive, and whatever happened to the RAM
     * ring afterwards, the first entries to land carry their identity. */
    if (archive_empty)
    {
        boot_lead = N_WW_LOG_BOOT_RECORD_SIZE;
        lead = boot_lead;
    }
#if CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_ERASE
    else
    {
        /* Reserve space without committing it yet. If this batch fits, the
         * marker/data are shifted down before append. If it crosses the end,
         * the reserved record becomes the first record after erase, preserving
         * the map identity of the restarted archive. */
        boot_lead = N_WW_LOG_BOOT_RECORD_SIZE;
        lead = boot_lead;
    }
#endif
#else
    archive_empty = 0;
#endif

#ifdef CONFIG_N_LOG_EXT_FLUSH_MARKER
    /* If a marker is armed for this drain, reserve its 8 bytes too; both leads
     * are only committed below once we know packed > 0, so an all-filtered
     * flush never emits lone control records and cannot spam the archive. */
    if (s_flush_marker_due)
    {
        lead += LOG_EXT_FLUSH_MARKER_SIZE;
    }
#endif

    /* Walk one staging buffer worth of whole entries, level-filtering into
     * `stage` (after any reserved marker); `consumed` counts every entry walked
     * so the RAM cursor advances past the filtered-out ones too. */
    packed = log_ram_pack_ext(stage + lead,
                              LOG_EXT_FLUSH_STAGE_SIZE - lead,
                              &consumed);
    if (consumed == 0)
    {
        log_mutex_unlock();
        return LOG_EXT_OK;            /* nothing walkable */
    }

    if (packed == 0)
    {
        lead = 0;                    /* all filtered: drop the reservations */
        boot_lead = 0;
    }
    else
    {
#if CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE
        if (archive_empty && boot_lead != 0)
        {
            (void)n_ww_log_fill_boot_record(stage);
        }
#endif
#ifdef CONFIG_N_LOG_EXT_FLUSH_MARKER
        if (s_flush_marker_due)
        {
            U32 *m = (U32 *)(stage + boot_lead);
            m[0] = LOG_EXT_FLUSH_MARKER_HDR;
            m[1] = (U32)xTaskGetTickCount();
            s_flush_marker_due = 0;
        }
#endif
    }
    total = lead + packed;

    /* Does the persisted batch (marker + entries) fit before the partition end? */
    end   = g_log_ext_ctx.log_offset + g_log_ext_ctx.log_size;
    avail = (g_log_ext_ctx.write_off < end) ? (end - g_log_ext_ctx.write_off) : 0;

#if (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE) && \
    (CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_ERASE)
    if (packed > 0 && !archive_empty && boot_lead != 0)
    {
        U16 normal_total = total - boot_lead;

        if (normal_total <= avail)
        {
            /* The batch fits without an erase, so discard the provisional boot
             * reservation. Source is above destination; forward word copies
             * are overlap-safe and every record is 4-byte aligned. */
            U16 i;
            for (i = 0; i < normal_total; i += 4)
            {
                *(U32 *)(stage + i) = *(U32 *)(stage + boot_lead + i);
            }
            total = normal_total;
            lead -= boot_lead;
            boot_lead = 0;
        }
        else
        {
            /* This batch starts a new archive after the erase below. */
            (void)n_ww_log_fill_boot_record(stage);
        }
    }
#endif

    if (total > avail)
    {
#if CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_FREEZE
        /* Freeze: this is the last batch the partition will take. Fill the tail
         * with as many WHOLE entries as still fit instead of discarding the
         * batch wholesale -- dropping it would strand up to one staging buffer
         * (255 B) of unused partition. Whatever does not fit is still consumed
         * from RAM so the ring keeps rolling for UART. */
        total = ext_whole_entry_prefix(stage, total, (U16)avail);
        g_log_ext_ctx.full = WW_TRUE;
        log_ram_set_ext_full();
        if (total == 0)
        {
            log_ram_consume(consumed);
            log_mutex_unlock();
            return LOG_EXT_ERR_PT_FULL;
        }
        dst_off = g_log_ext_ctx.write_off;
        g_log_ext_ctx.write_off += total;
        log_ram_consume(consumed);
        log_mutex_unlock();
        ret = ext_dev_write(dst_off, stage, total);
        return (ret == WW_OK) ? LOG_EXT_ERR_PT_FULL : LOG_EXT_ERR_WRITE_FAIL;
#else
        /* Erase: wipe + restart the stream, keeping these newest entries. The
         * device erase is deferred to outside the lock. */
        need_erase = 1;
        dst_off = g_log_ext_ctx.log_offset + LOG_EXT_PART_HDR_SIZE;
        g_log_ext_ctx.write_off = dst_off + total;
        g_log_ext_ctx.full = WW_FALSE;
#endif
    }
    else
    {
        dst_off = g_log_ext_ctx.write_off;
        g_log_ext_ctx.write_off += total;
    }

    log_ram_consume(consumed);
    log_mutex_unlock();

    /* Slow device I/O outside the lock (single-caller flush task owns write_off,
     * so the reserved region cannot be raced). */
    if (need_erase)
    {
        if (ext_partition_erase() != WW_OK)
        {
            return LOG_EXT_ERR_CLEAR_FAIL;
        }
        if (ext_parthdr_write() != WW_OK)
        {
            return LOG_EXT_ERR_WRITE_FAIL;
        }
    }
    if (total == 0)
    {
        return LOG_EXT_OK;           /* every entry filtered out: nothing to write */
    }
    ret = ext_dev_write(dst_off, stage, total);
    return (ret == WW_OK) ? LOG_EXT_OK : LOG_EXT_ERR_WRITE_FAIL;
}

#ifdef CONFIG_N_LOG_EXT_FLUSH_MARKER
void log_ext_flush_marker_arm(void)
{
    s_flush_marker_due = 1;
}
#endif

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
    return g_log_ext_ctx.write_off;
}

WW_BOOL log_ext_mem_is_full(void)
{
    /* FREEZE: full once the append stream reached the partition end -> stop
     * flushing to preserve the earliest logs.
     * ERASE: never full -- the partition is wiped and reused on overflow. */
#if CONFIG_N_LOG_EXT_FULL == N_WW_LOG_EXT_FULL_FREEZE
    return g_log_ext_ctx.full ? WW_TRUE : WW_FALSE;
#else
    return WW_FALSE;
#endif
}

U32 log_ext_mem_get_used(void)
{
    /* Bytes appended so far, i.e. header + entries. */
    if (g_log_ext_ctx.write_off <= g_log_ext_ctx.log_offset)
    {
        return 0;
    }
    return g_log_ext_ctx.write_off - g_log_ext_ctx.log_offset;
}

U32 log_ext_mem_get_remaining(void)
{
    U32 used = log_ext_mem_get_used();
    return (used < g_log_ext_ctx.log_size) ? (g_log_ext_ctx.log_size - used) : 0;
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
    ww_printf("Log part offset is 0x%X\n", g_log_ext_ctx.log_offset);
    ww_printf("Log offset is 0x%X\n", g_log_ext_ctx.log_offset + offset);

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
