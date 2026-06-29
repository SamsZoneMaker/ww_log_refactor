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
#include "n_ww_log_task.h"

/*************************** macro definition start ***************************/

/* RAM buffer layout */
#define LOG_RAM_HEADER_SIZE       sizeof(LOG_RAM_HEADER_T)  /* Header size in bytes */
#define LOG_RAM_DATA_SIZE         (DLM_MAINTAIN_LOG_SIZE - LOG_RAM_HEADER_SIZE)
#define LOG_RAM_FLUSH_THRESHOLD   (480)   /* ~one ext block payload (512B slot) */

#define LOG_RAM_MAGIC             (0x574C4F47)

#define LOG_FLAG_OVERFLOW         (1 << 0)
#define LOG_FLAG_ERROR            (1 << 1)
#define LOG_FLAG_FLUSH_NEEDED     (1 << 2)
#define LOG_FLAG_CORRUPTED        (1 << 3)

// support macro
#define LOG_SET_FLAG(flag)        (g_ram_buffer.header->flags |= (flag))
#define LOG_CLEAR_FLAG(flag)      (g_ram_buffer.header->flags &= ~(flag))
#define LOG_TEST_FLAG(flag)       ((g_ram_buffer.header->flags & (flag)) != 0)

#define LOG_CALC_STRUCT_CHECKSUM(ptr) \
    log_calc_checksum((ptr), sizeof(*(ptr)) - sizeof(U32))

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

/* ===================== External-storage container geometry =====================
 *
 * The LOG partition is laid out as a ring of fixed-size, self-describing blocks
 * plus a fixed control footer in the last slot:
 *
 *   [0 .. log_size-FOOTER) : N block slots, slot = LOGH header(32B) + payload
 *                            N = (log_size - FOOTER) / LOG_EXT_BLOCK_SIZE
 *   [log_size-FOOTER .. end): 32B footer = ring control (oldest/newest block,
 *                             write slot, wrap count, checksum). Fixed location
 *                             so the host always knows where to find it.
 *
 * Each flush packs WHOLE entries (never split mid-entry) up to PAYLOAD bytes
 * into one block, stamps a payload CRC, and writes it to the current ring slot.
 * On wrap the oldest slot is overwritten (RING) or flushing stops (FREEZE).
 */
#define LOG_EXTMEM_MAGIC          (0x474F4C46)   /* 'FLOG' - footer (ring control) */
#define LOG_BLOCK_MAGIC           (0x4C4F4748)   /* 'LOGH' - per-block header       */

#define LOG_EXT_BLOCK_SIZE        (512)                        /* one ring slot   */
#define LOG_EXT_BLOCK_HEADER_SIZE (sizeof(LOG_BLOCK_HEADER_T)) /* 28 bytes        */
#define LOG_EXT_FOOTER_SIZE       (sizeof(LOG_EXT_FOOTER_T))   /* 32 bytes        */
#define LOG_EXT_PAYLOAD_SIZE      (LOG_EXT_BLOCK_SIZE - LOG_EXT_BLOCK_HEADER_SIZE)
/* 4K partition: (4096-32)/512 = 7 ring slots (484B payload each), 480B unused
 * (3584B blocks + 32B footer = 3616, leaving 480B before the footer). */

/* Ext-full policy: RING overwrites the oldest block; FREEZE stops flushing.
 * Exactly one must be defined (default RING, set in autoconf.h). */
#if !defined(CONFIG_N_LOG_EXT_POLICY_RING) && !defined(CONFIG_N_LOG_EXT_POLICY_FREEZE)
#define CONFIG_N_LOG_EXT_POLICY_RING
#endif

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
                                 * bit3-15: reserved
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
 * Per-block header ('LOGH'), prepended to every flushed block (32 bytes).
 * Lets the host walk blocks independently and validate each payload by CRC,
 * so a torn/partial flush damages at most one block instead of desyncing the
 * whole partition.
 */
typedef struct
{
    U32 magic;         /*==< LOG_BLOCK_MAGIC 'LOGH'                         */
    U32 seq;           /*==< Monotonic block sequence (ordering after wrap) */
    U32 timestamp;     /*==< ww_cycle_get_32() at flush time                */
    U16 data_size;     /*==< Valid payload bytes in this block (<= PAYLOAD)  */
    U16 entry_count;   /*==< Number of whole entries packed in this block   */
    U32 crc;           /*==< CRC/checksum over the first data_size payload  */
    U32 reserved1;     /*==< */
    U32 reserved2;     /*==< */
} LOG_BLOCK_HEADER_T;  /*==< Total: 28 bytes (4+4+4+2+2+4+4+4); payload = 512-28 = 484 */

/* Log context in external storage (RAM-resident runtime state, not persisted) */
typedef struct
{
    U8  initialized;
    U8  ext_mem_type;
    U8  log_part_valid;
    U8  reserved;
    U32 log_offset;
    U32 log_size;
    U32 ext_write_offset;  /*==< byte offset of the current write slot       */
    U16 block_count;       /*==< number of ring slots = (log_size-32)/512    */
    U16 write_slot;        /*==< index of the slot to write next (0..N-1)    */
    U32 next_seq;          /*==< sequence number for the next block          */
    U32 wrap_count;        /*==< how many times the ring has wrapped         */
} LOG_EXT_CTX_T;

/**
 * Footer ('FLOG') = ring control block, written at the fixed tail slot
 * [log_size-32, log_size) every flush. The host reads it first to learn which
 * block is oldest/newest and how to reorder the ring (32 bytes).
 */
typedef struct
{
    U32 magic;                 /*==< LOG_EXTMEM_MAGIC 'FLOG'                 */
    U16 mem_type;              /*==< EXT_MEM_EEPROM / EXT_MEM_FLASH          */
    U16 write_slot;            /*==< next slot to write (newest = this-1)    */
    U32 wrap_count;            /*==< ring wrap count (0 = not yet wrapped)   */
    U32 next_seq;              /*==< next block sequence number              */
    U32 log_count;             /*==< entries persisted this boot             */
    U32 last_flush_timestamp;  /*==< */
    U32 reserved;              /*==< */
    U32 checksum;              /*==< sum of the first 28 bytes               */
} LOG_EXT_FOOTER_T;       /*==< Total: 32 bytes */

/*************************** type definition end *****************************/


/*************************** declaration start ***************************/

void log_ram_init(bool force_clear);
void log_ram_get_header_info(LOG_RAM_HEADER_T *info);
U32  log_calc_checksum(const void *data, U32 len);
WW_RTN log_ram_write(U32 encoded, U32 *params, U8 param_count);
void log_ram_mark_error(void);
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

/* RAM-ring -> ext bridge (implemented in n_ww_log_ram.c, called by the flush
 * driver in n_ww_log_storage.c under the log mutex). */
U16  log_ram_pack_block(U8 *dst, U16 *ecount);
void log_ram_consume(U16 packed);

WW_BOOL log_ram_is_need_flush(void);
U32  log_ram_get_threshold(void);
U32  log_ext_get_log_size(void);
U8   log_ext_get_mem_type(void);
U8   log_ext_get_initialized(void);
U8   log_ext_get_part_valid(void);
U32  log_ext_get_log_offset(void);
U32  log_ext_get_write_offset(void);
U16  log_ext_get_block_count(void);
U16  log_ext_get_write_slot(void);
U32  log_ext_get_wrap_count(void);
U32  log_ext_get_next_seq(void);
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
