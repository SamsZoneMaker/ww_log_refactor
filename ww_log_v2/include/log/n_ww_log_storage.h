/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* to add description for this file if needed */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_STORAGE_H__
#define __N_WW_LOG_STORAGE_H__

#ifdef __cplusplus
extern "C"
{
#endif

// #include <>
// #include ""
#include <stdbool.h>
#include "dlm_layout.h"
#include "n_ww_log_def.h"    /* encode layout + control-record namespace */
#include "n_ww_log_task.h"

/*************************** macro definition start ***************************/

/* RAM buffer layout */
#define LOG_RAM_HEADER_SIZE       sizeof(LOG_RAM_HEADER_T)  /* Header size in bytes */
#define LOG_RAM_DATA_SIZE         (DLM_MAINTAIN_LOG_SIZE - LOG_RAM_HEADER_SIZE)
/* Tuning knobs come from log_autoconf.h (generated from log_config.json), which
 * is force-included ahead of this header; the values here are the fallback for a
 * build that does not use the generator. */
#ifndef LOG_RAM_FLUSH_THRESHOLD
#define LOG_RAM_FLUSH_THRESHOLD   (480)   /* ~one ext block payload (512B slot) */
#endif

#define LOG_RAM_MAGIC             (0x574C4F47)

#define LOG_FLAG_OVERFLOW         (1 << 0)
#define LOG_FLAG_ERROR            (1 << 1)
#define LOG_FLAG_FLUSH_NEEDED     (1 << 2)
#define LOG_FLAG_CORRUPTED        (1 << 3)
/* FREEZE policy only: the archive filled up and stopped accepting entries.
 * Lives in the RAM header so it survives into a RAM dump -- otherwise "the
 * archive stops part-way through" is indistinguishable from "the device went
 * quiet", and the ext ctx that knows is RAM-resident and lost on a cold boot. */
#define LOG_FLAG_EXT_FULL         (1 << 4)

// support macro
#define LOG_SET_FLAG(flag)        (g_ram_buffer.header->flags |= (flag))
#define LOG_CLEAR_FLAG(flag)      (g_ram_buffer.header->flags &= ~(flag))
#define LOG_TEST_FLAG(flag)       ((g_ram_buffer.header->flags & (flag)) != 0)

#define LOG_CALC_STRUCT_CHECKSUM(ptr) \
    log_calc_checksum((ptr), sizeof(*(ptr)) - sizeof(U32))

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

/* The external backend has no storage of its own: it drains the RAM ring
 * (log_ram_pack_ext / log_ram_consume) under the ring's mutex. Selecting it
 * without the RAM backend used to fail at link time with a bare "undefined
 * reference to log_mutex_lock"; say so here instead. */
#ifndef CONFIG_N_LOG_BACKEND_RAM
#error "CONFIG_N_LOG_BACKEND_EXT_MEM requires CONFIG_N_LOG_BACKEND_RAM"
#endif

/* ================= External-storage container geometry (log-structured) =========
 *
 * The LOG partition is an append-only entry stream, NOT a fixed-slot ring:
 *
 *   [log_offset .. +8)          : 8B partition header ('XLOG'), written ONCE at
 *                                 first init and never rewritten. Distinguishes an
 *                                 initialized partition from erased/garbage bytes.
 *   [log_offset+8 .. write_off) : back-to-back whole entries (4B encoded header +
 *                                 pcnt*4B params), exactly the RAM-ring wire form.
 *   [write_off .. log_offset+log_size) : erased tail (0xFF).
 *
 * A flush copies whole entries whose level passes N_WW_LOG_EXT_LEVEL_THRESHOLD
 * from the RAM ring and appends them at write_off. No per-block header, no CRC,
 * no footer: the stream is self-describing (each entry's pcnt gives its length)
 * and the first 0xFFFFFFFF word marks the end.
 *
 * Why log-structured: NOR flash cannot erase sub-sector slots, so a 512B slot
 * ring is impossible on flash; and a periodic (timer) flush of a few bytes wasted
 * a whole 512B slot + 28B header. Appending writes only the bytes produced and
 * only erases (the whole partition) when it fills -- rare, since the level
 * threshold keeps ext traffic small. write_off lives in the RAM-resident ctx
 * (noinit): a hot restart keeps it; a cold boot rebuilds it by scanning to the
 * first 0xFFFFFFFF (see ext_scan_write_off).
 */
#define LOG_EXTMEM_MAGIC          (0x474F4C58)   /* 'XLOG' - partition header magic */
#define LOG_EXT_FORMAT_VERSION    (1)

#define LOG_EXT_PART_HDR_SIZE     (sizeof(LOG_EXT_PART_HDR_T))  /* 8 bytes */
#define LOG_EXT_ERASED_WORD       (0xFFFFFFFFu)  /* end-of-stream / erased marker */

/* Per-flush staging buffer: bounds both the RAM bytes scanned per log_ram_flush()
 * call and the static append buffer. The flush task re-arms while data remains,
 * so a backlog drains over successive calls. Must exceed one max entry (64B). */
#ifndef LOG_EXT_FLUSH_STAGE_SIZE
#define LOG_EXT_FLUSH_STAGE_SIZE  (256)
#endif

/* Ext-full policy: FREEZE stops flushing (preserves the earliest logs); ERASE
 * wipes the partition and restarts (preserves the newest). log-structured has no
 * per-slot rolling window, so it is one or the other. Exactly one must be
 * defined (default FREEZE, overridable in autoconf.h). */
#if !defined(CONFIG_N_LOG_EXT_FULL_FREEZE) && !defined(CONFIG_N_LOG_EXT_FULL_ERASE)
#define CONFIG_N_LOG_EXT_FULL_FREEZE
#endif

/* Flush-batch marker (enable with CONFIG_N_LOG_EXT_FLUSH_MARKER in autoconf.h).
 * An 8-byte record [LOG_EXT_FLUSH_MARKER_HDR][U32 tick] is prepended to the first
 * data-bearing flush of each flush-task wake, so the host decoder can split the
 * append stream into per-flush batches AND spot reboots (the tick resets to ~0).
 * It is a control record (see n_ww_log_def.h): file_id 0xFFF is reserved from
 * ever being assigned to a source file, so the pair cannot collide with a real
 * call site, it is distinct from the 0xFFFFFFFF erased word, and its pcnt=1
 * makes the generic entry walker and the cold-boot scan skip it transparently
 * as a normal 8-byte entry. */
#define LOG_EXT_FLUSH_MARKER_HDR \
    N_WW_LOG_ENCODE(N_WW_LOG_CTRL_FILE_ID, N_WW_LOG_CTRL_LINE_FLUSH, \
                    N_WW_LOG_LEVEL_ERR, 1)          /* == 0xFFFFFFC1 */
#define LOG_EXT_FLUSH_MARKER_SIZE (8)   /* header U32 + tick U32 */

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */

/*************************** macro definition end *****************************/

/*************************** type definition start ***************************/

/**
 * RAM Log Header (32 bytes)
 * Located at DLM_MAINTAIN_LOG_BASE_ADDR
 */
typedef struct
{
    U32 magic;             /*==< Magic number: 0x574C4F47 ('WLOG') */
    U16 write_index;       /*==< Write pointer (byte offset in data area: 0~DATA_SIZE-1) */
    U16 read_index;        /*==< Read pointer (byte offset in data area: 0~DATA_SIZE-1) */
    U16 pending_len;       /*==< Length of data to be refreshed to external storage (in bytes) */
    U16 flush_count;       /*==< Total number of flush(for debug and count) */
    U16 overflow_count;    /*==< */
    U16 flags;             /*==< Flag:
                                 * bit0: overflow_flag
                                 * bit1: error_flag
                                 * bit2: flush_needed
                                 * bit3: corrupted
                                 * bit4: ext archive full (FREEZE)
                                 * bit5-15: reserved
                                 */
    U32 log_count;         /*==< */
    U32 reserved1;         /*==< */
    U32 reserved2;         /*==< */
    U32 checksum;          /*==< Checksum: The simple sum of the first 28 bytes */
} LOG_RAM_HEADER_T;        /*==< Total: 32 bytes */

/**
 * Ring Buffer management structure
 */
typedef struct
{
    LOG_RAM_HEADER_T *header; /*==< Pointer to header */
    U8  *data;                /*==< Pointer to data area */
    U16 data_size;            /*==< Data area size (4064 bytes) */
    U16 threshold;           /*==< Flush threshold (LOG_RAM_FLUSH_THRESHOLD = 480 bytes) */
} LOG_RAM_BUFFER_T;

/* Err Code definition */
typedef enum
{
    LOG_EXT_OK                 = 0,
    LOG_EXT_ERR_NO_EXT_MEM     = 1,
    LOG_EXT_ERR_PT_NULL        = 2,
    LOG_EXT_ERR_PT_INVALID     = 3,
    LOG_EXT_ERR_NO_LOG_PART    = 4,
    LOG_EXT_ERR_WRITE_FAIL     = 5,
    LOG_EXT_ERR_READ_FAIL      = 6,
    LOG_EXT_ERR_PT_FULL        = 7,
    LOG_EXT_ERR_MUTEX_FAIL     = 8,
    LOG_EXT_ERR_CLEAR_FAIL = 9,

    LOG_EXT_ERR
} LOG_EXT_ERR_E;

/* External memory */
typedef enum
{
    EXT_MEM_NONE = 0,
    EXT_MEM_EEPROM = 1,
    EXT_MEM_FLASH = 2,
} EXT_MEM_TYPE_E;

/**
 * Partition header ('XLOG'), written ONCE at the partition base on first init
 * and never rewritten (8 bytes). A cold boot reads it to tell an initialized
 * partition (resume by scanning) from erased/garbage bytes (start fresh).
 */
typedef struct
{
    U32 magic;         /*==< LOG_EXTMEM_MAGIC 'XLOG'                        */
    U16 version;       /*==< LOG_EXT_FORMAT_VERSION                         */
    U16 reserved;      /*==< */
} LOG_EXT_PART_HDR_T;  /*==< Total: 8 bytes */

/* Log context in external storage (RAM-resident runtime state, not persisted;
 * on the target this lives in the noinit region so a hot restart keeps it). */
typedef struct
{
    U8  initialized;
    U8  ext_mem_type;
    U8  log_part_valid;
    U8  full;              /*==< FREEZE: set once the partition filled up    */
    U32 log_offset;        /*==< partition base (absolute device offset)     */
    U32 log_size;          /*==< partition size in bytes                     */
    U32 write_off;         /*==< absolute offset where the next entry is written */
} LOG_EXT_CTX_T;

/*************************** type definition end *****************************/


/*************************** declaration start ***************************/

void log_ram_init(bool force_clear);
void log_ram_get_header_info(LOG_RAM_HEADER_T *info);
U32  log_calc_checksum(const void *data, U32 len);
WW_RTN log_ram_write(U32 encoded, U32 *params, U8 param_count);
void log_ram_mark_error(void);
#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
void log_ram_set_ext_full(void);   /* FREEZE: archive filled up */
#endif
WW_RTN log_ram_validate_data(void);
void log_ram_dump_hex(void);
void log_ram_index_reset(void);

U8   log_ram_validate(void);
U16  get_current_usage(void);
U16  log_ram_get_available(void);
U16  log_ram_get_data_size(void);
U16  log_ram_get_write_index(void);
U16  log_ram_get_read_index(void);
U16  log_ram_get_pending_len(void);
U8* log_ram_get_data_ptr(void);
U32  log_ram_get_log_count(void);
U16  log_ram_get_overflow_count(void);
U16  log_ram_get_flags(void);

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

WW_RTN log_ext_mem_init(void);
int  log_ext_mem_available(void);
int  log_ext_mem_read(U8 *buf, U32 len);
void log_ext_mem_dump(void);
int  log_ext_mem_clear(void);
void log_ext_force_reinit(void);   /* drop ext ctx -> next access re-inits (reboot sim/test) */
int  log_ram_flush(void);

#ifdef CONFIG_N_LOG_EXT_FLUSH_MARKER
/* Arm a flush marker for the current drain: the next data-bearing log_ram_flush()
 * prepends one [LOG_EXT_FLUSH_MARKER_HDR][tick] record. Call once per flush-task
 * wake so each wake's persisted batch carries a single timestamp. */
void log_ext_flush_marker_arm(void);
#endif

/* RAM-ring -> ext bridge (implemented in n_ww_log_ram.c, called by the flush
 * driver in n_ww_log_storage.c under the log mutex). log_ram_pack_ext copies
 * whole entries whose level passes the ext threshold into dst (up to `budget`
 * RAM bytes scanned), returning the packed byte count and, via *consumed, how
 * many RAM bytes were walked (which log_ram_consume then advances past). */
U16  log_ram_pack_ext(U8 *dst, U16 budget, U16 *consumed);
void log_ram_consume(U16 consumed);

WW_BOOL log_ram_is_need_flush(void);
U32  log_ram_get_threshold(void);
U32  log_ext_get_log_size(void);
U8   log_ext_get_mem_type(void);
U8   log_ext_get_initialized(void);
U8   log_ext_get_part_valid(void);
U32  log_ext_get_log_offset(void);
U32  log_ext_get_write_offset(void);
WW_BOOL log_ext_mem_is_full(void);
U32  log_ext_mem_get_used(void);
U32  log_ext_mem_get_remaining(void);
U32  log_ram_get_flush_count(void);

#endif // CONFIG_N_LOG_BACKEND_EXT_MEM

static inline U16 log_ram_get_next_index(U16 current_index)
{
    U16 next_index = current_index + sizeof(U32);

    if (next_index >= LOG_RAM_DATA_SIZE)
    {
        next_index -= LOG_RAM_DATA_SIZE;
    }

    return next_index;
}

/*************************** declaration end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_STORAGE_H__ */
