/**
 * @file ww_log_store.h
 * @brief Log persistence: RAM ring-buffer, external storage, block headers, and flush.
 *
 * Three layers cooperate to move encoded entries from volatile RAM to durable
 * storage. The RAM ring-buffer (log_ram_*) holds freshly encoded entries in the
 * 4 KB DLM maintain region that survives power-loss. When usage crosses
 * LOG_RAM_FLUSH_THRESHOLD the flush engine (log_flush_*) drains the buffer to
 * external storage, prepending a 32-byte block header (LOG_BLOCK_HEADER_T) for
 * framing and sequencing. The storage layer (log_storage_*) abstracts EEPROM /
 * Flash access; in SIMULATION_MODE it redirects to sim_storage.*
 */

#ifndef WW_LOG_STORE_H
#define WW_LOG_STORE_H

#include "type.h"
#include "ww_log_config.h"

/* ============================================================
 * RAM ring-buffer (log_ram_*)
 * ============================================================ */

/** RAM LOG header (64 bytes), located at DLM_MAINTAIN_LOG_BASE_ADDR. */
typedef struct {
    U32 magic;              /**< Magic number: LOG_RAM_MAGIC ('WLOG') */
    U32 version;            /**< Version: LOG_RAM_VERSION */
    U16 write_index;        /**< Write pointer (byte offset in data area) */
    U16 read_index;         /**< Read pointer (byte offset in data area) */
    U32 total_written;      /**< Total bytes ever written (running) */
    U32 flush_count;        /**< Number of flushes to external storage */
    U32 last_flush_time;    /**< Last flush timestamp (optional) */
    U8  overflow_flag;      /**< 1 = ring wrapped around */
    U8  reserved[35];       /**< Reserved */
    U32 checksum;           /**< Checksum of the first 60 bytes */
} LOG_RAM_HEADER_T;

/** Ring-buffer management handle. */
typedef struct {
    LOG_RAM_HEADER_T *header;
    U8  *data;
    U16  data_size;
    U16  threshold;
} LOG_RAM_BUFFER_T;

#ifdef LOG_RAM_STATISTICS
typedef struct {
    U32 write_calls;
    U32 write_bytes;
    U32 flush_triggers;
    U32 overflow_count;
    U16 peak_usage;
} LOG_RAM_STATS_T;
#endif

void log_ram_init(U8 force_clear);
int  log_ram_write(U32 encoded, U32 *params, U8 param_count);
U8   log_ram_need_flush(void);
U16  log_ram_get_usage(void);
U16  log_ram_get_available(void);
int  log_ram_read(U8 *buffer, U16 max_size, U16 *actual_size);
void log_ram_clear_flushed(U16 size);
void log_ram_clear_all(void);
const LOG_RAM_HEADER_T *log_ram_get_header(void);
#ifdef LOG_RAM_STATISTICS
void log_ram_get_stats(LOG_RAM_STATS_T *stats);
#endif
void log_ram_dump_hex(void);
U8   log_ram_validate(void);
U32  log_ram_calc_checksum(const LOG_RAM_HEADER_T *header);

/* ============================================================
 * External storage (log_storage_*)
 * ============================================================ */

typedef enum {
    EXT_MEM_NONE   = REG_WW_STUS_SYS_INFO_EXT_MEM_NONE,
    EXT_MEM_EEPROM = REG_WW_STUS_SYS_INFO_EXT_MEM_EEPROM,
    EXT_MEM_FLASH  = REG_WW_STUS_SYS_INFO_EXT_MEM_FLASH
} EXT_MEM_TYPE_E;

typedef struct {
    U32 part_offset;
    U32 part_size;
    U8  part_type;
    U8  disk_type;
    U8  part_id;
    U8  reserved;
} PART_ENTRY_T;

typedef struct {
    U32 magic;
    U16 entry_count;
    U16 reserved;
    PART_ENTRY_T entries[16];
} PART_TABLE_T;

typedef struct {
    PART_TABLE_T pt_info;
} LAUNCH_INFO_T;

#ifdef SIMULATION_MODE

typedef struct {
    U8 extMemType;
    U8 reserved[3];
} SIM_SYS_INFO_T;

extern SIM_SYS_INFO_T g_sim_sys_info;
#define REG_WW_STUS_SYS_INFO_U g_sim_sys_info

PART_TABLE_T  *sim_pt_info_read(void);
PART_ENTRY_T  *sim_pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2);
U8             sim_pt_table_check_valid(PART_TABLE_T *pt);
LAUNCH_INFO_T *sim_dlm_data_launch_info_get(void);

#define pt_info_read()           sim_pt_info_read()
#define pt_entry_get_by_key      sim_pt_entry_get_by_key
#define pt_table_check_valid     sim_pt_table_check_valid
#define dlm_data_launch_info_get sim_dlm_data_launch_info_get

int sim_eeprom_write(U32 offset, const U8 *data, U32 size);
int sim_eeprom_read(U32 offset, U8 *data, U32 size);
int sim_flash_write(U32 offset, const U8 *data, U32 size);
int sim_flash_read(U32 offset, U8 *data, U32 size);
int sim_flash_erase(U32 offset, U32 size);

#define svc_eeprom_acc_write sim_eeprom_write
#define svc_eeprom_acc_read  sim_eeprom_read
#define svc_flash_acc_write  sim_flash_write
#define svc_flash_acc_read   sim_flash_read

#else

PART_TABLE_T  *pt_info_read(void);
PART_ENTRY_T  *pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2);
U8             pt_table_check_valid(PART_TABLE_T *pt);
LAUNCH_INFO_T *dlm_data_launch_info_get(void);

int svc_eeprom_acc_write(U32 offset, const U8 *data, U32 size);
int svc_eeprom_acc_read(U32 offset, U8 *data, U32 size);
int svc_flash_acc_write(U32 vOfst, const U8 *data, U32 size);
int svc_flash_acc_read(U32 vOfst, U8 *data, U32 size);

#endif /* SIMULATION_MODE */

int            log_storage_init(void);
EXT_MEM_TYPE_E log_storage_detect_type(void);
PART_TABLE_T  *log_storage_get_partition_table(void);
U8             log_storage_check_partition_valid(PART_TABLE_T *pt);
PART_ENTRY_T  *log_storage_get_log_partition(void);
int            log_storage_write(U32 offset, const U8 *data, U32 size);
int            log_storage_read(U32 offset, U8 *data, U32 size);
int            log_storage_erase(U32 offset, U32 size);
EXT_MEM_TYPE_E log_storage_get_current_type(void);
int            log_storage_get_partition_info(U32 *offset, U32 *size);
U8             log_storage_is_available(void);

/* ============================================================
 * Block header (log_header_*)
 * ============================================================ */

/** External-storage LOG block header (32 bytes). */
typedef struct {
    U32 magic;              /**< LOG_BLOCK_MAGIC ('LOGH') */
    U32 sequence;           /**< Incrementing sequence number */
    U32 timestamp;          /**< System tick (optional) */
    U16 data_size;          /**< Bytes of data in this block */
    U16 entry_count;        /**< Approximate number of LOG entries */
    U8  ram_overflow;       /**< RAM overflow flag at flush time */
    U8  reserved[11];
    U32 checksum;           /**< Checksum of the first 28 bytes */
} LOG_BLOCK_HEADER_T;

void log_header_init(void);
U32  log_header_get_next_sequence(void);
U32  log_header_get_current_sequence(void);
int  log_header_build(LOG_BLOCK_HEADER_T *header, U16 data_size,
                      U16 entry_count, U8 ram_overflow);
U8   log_header_validate(const LOG_BLOCK_HEADER_T *header);
U32  log_header_calc_checksum(const LOG_BLOCK_HEADER_T *header);
U32  log_header_scan_max_sequence(void);

/* ============================================================
 * Flush engine (log_flush_*)
 * ============================================================ */

typedef enum {
    FLUSH_STATUS_IDLE = 0,
    FLUSH_STATUS_PENDING,
    FLUSH_STATUS_IN_PROGRESS,
    FLUSH_STATUS_COMPLETED,
    FLUSH_STATUS_FAILED
} FLUSH_STATUS_E;

void           log_flush_init(void);
int            log_flush_request(U8 force);
int            log_flush_process(void);
FLUSH_STATUS_E log_flush_get_status(void);
U8             log_flush_is_busy(void);
int            log_flush_now(void);
void           log_flush_get_stats(U32 *total_flushes, U32 *failed_flushes,
                                   U16 *last_flush_size);

#endif /* WW_LOG_STORE_H */
