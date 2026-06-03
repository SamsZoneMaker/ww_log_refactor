/**
 * @file ww_log_store.c
 * @brief Log persistence: RAM ring-buffer, external storage, block headers, flush.
 */

#include "ww_log_config.h"
#include "ww_log_store.h"
#include <string.h>
#include <stdio.h>

/* ============================================================
 * RAM ring-buffer (log_ram_*)        gated on WW_LOG_BACKEND_RAM
 * ============================================================ */
#if (WW_LOG_BACKEND_RAM == 1)

#ifdef SIMULATION_MODE
/* "Power-loss retained" maintain region, simulated as a static array. */
U8 g_sim_dlm_memory[4096] = {0};
#endif

static LOG_RAM_BUFFER_T g_ram_buffer = {0};

#ifdef LOG_RAM_STATISTICS
static LOG_RAM_STATS_T g_ram_stats = {0};
#endif

U32 log_ram_calc_checksum(const LOG_RAM_HEADER_T *header)
{
    U32 checksum = 0;
    const U8 *data = (const U8 *)header;
    U16 i;
    for (i = 0; i < (sizeof(LOG_RAM_HEADER_T) - sizeof(U32)); i++) {
        checksum += data[i];
    }
    return checksum;
}

static U8 validate_header(const LOG_RAM_HEADER_T *header)
{
    if (header->magic != LOG_RAM_MAGIC) return 0;
    if (header->version != LOG_RAM_VERSION) return 0;
    if (header->write_index >= LOG_RAM_DATA_SIZE ||
        header->read_index  >= LOG_RAM_DATA_SIZE) return 0;
    if (log_ram_calc_checksum(header) != header->checksum) return 0;
    return 1;
}

static void init_header(LOG_RAM_HEADER_T *header)
{
    memset(header, 0, sizeof(LOG_RAM_HEADER_T));
    header->magic    = LOG_RAM_MAGIC;
    header->version  = LOG_RAM_VERSION;
    header->checksum = log_ram_calc_checksum(header);
}

static void update_checksum(LOG_RAM_HEADER_T *header)
{
    header->checksum = log_ram_calc_checksum(header);
}

static U16 get_current_usage(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    if (header->write_index >= header->read_index)
        return header->write_index - header->read_index;
    return g_ram_buffer.data_size - header->read_index + header->write_index;
}

void log_ram_init(U8 force_clear)
{
    g_ram_buffer.header    = (LOG_RAM_HEADER_T *)DLM_MAINTAIN_LOG_BASE_ADDR;
    g_ram_buffer.data      = (U8 *)(DLM_MAINTAIN_LOG_BASE_ADDR + LOG_RAM_HEADER_SIZE);
    g_ram_buffer.data_size = LOG_RAM_DATA_SIZE;
    g_ram_buffer.threshold = LOG_RAM_FLUSH_THRESHOLD;

    U8 valid = 0;
    if (!force_clear) {
        valid = validate_header(g_ram_buffer.header);
#ifdef LOG_DEBUG_VERBOSE
        if (valid)
            printf("LOG_RAM: valid header, preserving data "
                   "(w=%u r=%u total=%u flushes=%u)\n",
                   g_ram_buffer.header->write_index,
                   g_ram_buffer.header->read_index,
                   g_ram_buffer.header->total_written,
                   g_ram_buffer.header->flush_count);
#endif
    }

    if (!valid || force_clear) {
        init_header(g_ram_buffer.header);
        memset(g_ram_buffer.data, 0, g_ram_buffer.data_size);
#ifdef LOG_DEBUG_VERBOSE
        printf("LOG_RAM: initialized (force_clear=%u)\n", force_clear);
#endif
    }

#ifdef LOG_RAM_STATISTICS
    memset(&g_ram_stats, 0, sizeof(g_ram_stats));
#endif
}

int log_ram_write(U32 encoded, U32 *params, U8 param_count)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 write_idx = header->write_index;
    U16 required  = (U16)(4 + param_count * 4);
    U16 available = log_ram_get_available();
    U8  i;

    if (required > available) {
        if (required > g_ram_buffer.data_size)
            return -1;
        header->overflow_flag = 1;
        write_idx = 0;
        header->write_index = 0;
#ifdef LOG_RAM_STATISTICS
        g_ram_stats.overflow_count++;
#endif
    }

    *(U32 *)(g_ram_buffer.data + write_idx) = encoded;
    write_idx += 4;
    for (i = 0; i < param_count; i++) {
        *(U32 *)(g_ram_buffer.data + write_idx) = params[i];
        write_idx += 4;
    }

    header->write_index   = write_idx;
    header->total_written += required;
    update_checksum(header);

#ifdef LOG_RAM_STATISTICS
    g_ram_stats.write_calls++;
    g_ram_stats.write_bytes += required;
    {
        U16 usage = get_current_usage();
        if (usage > g_ram_stats.peak_usage)
            g_ram_stats.peak_usage = usage;
    }
#endif

    if (log_ram_need_flush()) {
#ifdef LOG_RAM_STATISTICS
        g_ram_stats.flush_triggers++;
#endif
        return 1;
    }
    return 0;
}

U8 log_ram_need_flush(void)
{
    return (get_current_usage() >= g_ram_buffer.threshold) ? 1 : 0;
}

U16 log_ram_get_usage(void)     { return get_current_usage(); }

U16 log_ram_get_available(void) { return g_ram_buffer.data_size - get_current_usage(); }

int log_ram_read(U8 *buffer, U16 max_size, U16 *actual_size)
{
    if (buffer == NULL || actual_size == NULL) return -1;

    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 read_idx  = header->read_index;
    U16 write_idx = header->write_index;
    U16 available = get_current_usage();
    U16 to_copy   = (available < max_size) ? available : max_size;

    if (to_copy == 0) { *actual_size = 0; return 0; }

    if (write_idx > read_idx) {
        memcpy(buffer, g_ram_buffer.data + read_idx, to_copy);
    } else {
        U16 first_part = g_ram_buffer.data_size - read_idx;
        if (to_copy <= first_part) {
            memcpy(buffer, g_ram_buffer.data + read_idx, to_copy);
        } else {
            memcpy(buffer, g_ram_buffer.data + read_idx, first_part);
            memcpy(buffer + first_part, g_ram_buffer.data,
                   (U16)(to_copy - first_part));
        }
    }

    *actual_size = to_copy;
    return 0;
}

void log_ram_clear_flushed(U16 size)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    header->read_index = (U16)((header->read_index + size) % g_ram_buffer.data_size);
    if (header->read_index == header->write_index) {
        header->read_index  = 0;
        header->write_index = 0;
        header->overflow_flag = 0;
    }
    header->flush_count++;
    update_checksum(header);
}

void log_ram_clear_all(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    header->write_index   = 0;
    header->read_index    = 0;
    header->overflow_flag = 0;
    update_checksum(header);
    memset(g_ram_buffer.data, 0, g_ram_buffer.data_size);
}

const LOG_RAM_HEADER_T *log_ram_get_header(void)
{
    return g_ram_buffer.header;
}

#ifdef LOG_RAM_STATISTICS
void log_ram_get_stats(LOG_RAM_STATS_T *stats)
{
    if (stats != NULL)
        memcpy(stats, &g_ram_stats, sizeof(LOG_RAM_STATS_T));
}
#endif

void log_ram_dump_hex(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 usage = get_current_usage();
    U16 i, j;

    printf("\n===== RAM LOG BUFFER DUMP =====\n");
    printf("Magic: 0x%08X %s\n", header->magic,
           (header->magic == LOG_RAM_MAGIC) ? "(VALID)" : "(INVALID)");
    printf("Version: 0x%08X\n", header->version);
    printf("Write/Read index: %u / %u\n", header->write_index, header->read_index);
    printf("Usage: %u/%u bytes\n", usage, g_ram_buffer.data_size);
    printf("Total written: %u bytes, flushes: %u, overflow: %u\n",
           header->total_written, header->flush_count, header->overflow_flag);
    printf("Checksum: 0x%08X\n", header->checksum);
    printf("-------------------------------\n");

    if (usage > 0) {
        U16 to_print = (usage > 256) ? 256 : usage;
        U16 read_idx = header->read_index;
        for (i = 0; i < to_print; i += 16) {
            printf("%04X: ", i);
            for (j = 0; j < 16 && (i + j) < to_print; j++) {
                U16 idx = (U16)((read_idx + i + j) % g_ram_buffer.data_size);
                printf("%02X ", g_ram_buffer.data[idx]);
            }
            printf("\n");
        }
    }
    printf("===============================\n\n");
}

U8 log_ram_validate(void)
{
    return validate_header(g_ram_buffer.header);
}

#endif /* WW_LOG_BACKEND_RAM */


/* ============================================================
 * External storage (log_storage_*)  gated on WW_LOG_BACKEND_STORAGE
 * ============================================================ */
#if (WW_LOG_BACKEND_STORAGE == 1)

static EXT_MEM_TYPE_E  g_ext_mem_type        = EXT_MEM_NONE;
static PART_TABLE_T   *g_partition_table      = NULL;
static PART_ENTRY_T   *g_log_partition        = NULL;
static U8              g_storage_initialized  = 0;

static PART_ENTRY_T *find_log_partition(PART_TABLE_T *pt)
{
    if (pt == NULL) return NULL;
    return pt_entry_get_by_key(pt, PART_ENTRY_TYPE_LOG, 0, 0);
}

int log_storage_init(void)
{
    g_ext_mem_type = log_storage_detect_type();
    if (g_ext_mem_type == EXT_MEM_NONE) return -1;

    g_partition_table = log_storage_get_partition_table();
    if (g_partition_table == NULL) return -1;
    if (!log_storage_check_partition_valid(g_partition_table)) return -1;

    g_log_partition = find_log_partition(g_partition_table);
    if (g_log_partition == NULL) return -1;

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_STORAGE: LOG partition offset=0x%08X size=%u type=%u\n",
           g_log_partition->part_offset, g_log_partition->part_size,
           g_log_partition->part_type);
#endif

    g_storage_initialized = 1;
    return 0;
}

EXT_MEM_TYPE_E log_storage_detect_type(void)
{
#ifdef SIMULATION_MODE
    return (EXT_MEM_TYPE_E)g_sim_sys_info.extMemType;
#else
    return (EXT_MEM_TYPE_E)REG_WW_STUS_SYS_INFO_U.extMemType;
#endif
}

PART_TABLE_T *log_storage_get_partition_table(void)   { return pt_info_read(); }

U8 log_storage_check_partition_valid(PART_TABLE_T *pt) { return pt_table_check_valid(pt); }

PART_ENTRY_T *log_storage_get_log_partition(void)     { return g_log_partition; }

int log_storage_write(U32 offset, const U8 *data, U32 size)
{
    int ret;
    U32 abs_offset;

    if (!g_storage_initialized || g_log_partition == NULL ||
        data == NULL || size == 0) return -1;
    if (offset + size > g_log_partition->part_size) return -1;

    abs_offset = g_log_partition->part_offset + offset;

    if (g_ext_mem_type == EXT_MEM_EEPROM)
        ret = svc_eeprom_acc_write(abs_offset, data, size);
    else if (g_ext_mem_type == EXT_MEM_FLASH)
        ret = svc_flash_acc_write(abs_offset, data, size);
    else
        return -1;

    if (ret != 0) {
        int i;
        for (i = 0; i < LOG_STORAGE_WRITE_RETRY; i++) {
            ret = (g_ext_mem_type == EXT_MEM_EEPROM)
                ? svc_eeprom_acc_write(abs_offset, data, size)
                : svc_flash_acc_write(abs_offset, data, size);
            if (ret == 0) break;
        }
    }
    return ret;
}

int log_storage_read(U32 offset, U8 *data, U32 size)
{
    U32 abs_offset;

    if (!g_storage_initialized || g_log_partition == NULL ||
        data == NULL || size == 0) return -1;
    if (offset + size > g_log_partition->part_size) return -1;

    abs_offset = g_log_partition->part_offset + offset;

    if (g_ext_mem_type == EXT_MEM_EEPROM)
        return svc_eeprom_acc_read(abs_offset, data, size);
    else if (g_ext_mem_type == EXT_MEM_FLASH)
        return svc_flash_acc_read(abs_offset, data, size);
    return -1;
}

int log_storage_erase(U32 offset, U32 size)
{
    U32 abs_offset;

    if (!g_storage_initialized || g_log_partition == NULL) return -1;
    if (g_ext_mem_type == EXT_MEM_EEPROM) return 0;   /* EEPROM has no erase */
    if (offset + size > g_log_partition->part_size) return -1;

    abs_offset = g_log_partition->part_offset + offset;

    if (g_ext_mem_type == EXT_MEM_FLASH) {
#ifdef SIMULATION_MODE
        return sim_flash_erase(abs_offset, size);
#else
        return 0;
#endif
    }
    return -1;
}

EXT_MEM_TYPE_E log_storage_get_current_type(void) { return g_ext_mem_type; }

int log_storage_get_partition_info(U32 *offset, U32 *size)
{
    if (!g_storage_initialized || g_log_partition == NULL) return -1;
    if (offset) *offset = g_log_partition->part_offset;
    if (size)   *size   = g_log_partition->part_size;
    return 0;
}

U8 log_storage_is_available(void)
{
    return (g_storage_initialized &&
            g_ext_mem_type != EXT_MEM_NONE &&
            g_log_partition != NULL) ? 1 : 0;
}

/* ============================================================
 * Block header (log_header_*)        gated on WW_LOG_BACKEND_STORAGE
 * ============================================================ */

static U32 g_sequence_counter  = 0;
static U8  g_header_initialized = 0;

static U32 get_timestamp(void)
{
    /* TODO: hook to RTC / system tick on real hardware. */
    return 0;
}

void log_header_init(void)
{
    if (g_header_initialized) return;
    g_sequence_counter = log_header_scan_max_sequence() + 1;
#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_HEADER: starting sequence=%u\n", g_sequence_counter);
#endif
    g_header_initialized = 1;
}

U32 log_header_get_next_sequence(void)    { return g_sequence_counter++; }
U32 log_header_get_current_sequence(void) { return g_sequence_counter; }

int log_header_build(LOG_BLOCK_HEADER_T *header, U16 data_size,
                     U16 entry_count, U8 ram_overflow)
{
    if (header == NULL) return -1;
    memset(header, 0, sizeof(LOG_BLOCK_HEADER_T));
    header->magic        = LOG_BLOCK_MAGIC;
    header->sequence     = log_header_get_next_sequence();
    header->timestamp    = get_timestamp();
    header->data_size    = data_size;
    header->entry_count  = entry_count;
    header->ram_overflow = ram_overflow;
    header->checksum     = log_header_calc_checksum(header);
    return 0;
}

U8 log_header_validate(const LOG_BLOCK_HEADER_T *header)
{
    if (header == NULL || header->magic != LOG_BLOCK_MAGIC) return 0;
    if (header->data_size > LOG_STORAGE_PARTITION_SIZE - sizeof(LOG_BLOCK_HEADER_T))
        return 0;
    if (log_header_calc_checksum(header) != header->checksum) return 0;
    return 1;
}

U32 log_header_calc_checksum(const LOG_BLOCK_HEADER_T *header)
{
    U32 checksum = 0;
    const U8 *data = (const U8 *)header;
    U16 i;
    for (i = 0; i < (sizeof(LOG_BLOCK_HEADER_T) - sizeof(U32)); i++)
        checksum += data[i];
    return checksum;
}

U32 log_header_scan_max_sequence(void)
{
    U32 max_sequence = 0;
    U32 off = 0;
    U32 part_off, part_size;
    LOG_BLOCK_HEADER_T header;

    if (!log_storage_is_available()) return 0;
    if (log_storage_get_partition_info(&part_off, &part_size) != 0) return 0;

    while (off + sizeof(header) <= part_size) {
        if (log_storage_read(off, (U8 *)&header, sizeof(header)) != 0) break;
        if (!log_header_validate(&header)) break;
        if (header.sequence > max_sequence)
            max_sequence = header.sequence;
        off += sizeof(header) + header.data_size;
    }
    return max_sequence;
}

/* ============================================================
 * Flush engine (log_flush_*)         gated on WW_LOG_BACKEND_STORAGE
 * ============================================================ */

static FLUSH_STATUS_E g_flush_status    = FLUSH_STATUS_IDLE;
static U8             g_flush_force     = 0;
static U32            g_total_flushes   = 0;
static U32            g_failed_flushes  = 0;
static U16            g_last_flush_size = 0;

/* Block-ring write cursor: byte offset into the LOG partition. */
static U32 g_storage_write_off = 0;

/* Static scratch buffer (no dynamic memory on target). */
static U8 g_flush_buffer[LOG_STORAGE_PARTITION_SIZE];

static U16 count_log_entries(const U8 *buffer, U16 size)
{
    U16 i = 0, n = 0;
    while (i + 4 <= size) {
        U32 hdr  = *(const U32 *)(buffer + i);
        U8  pcnt = (U8)(hdr & 0x3F);
        U16 entry_len = (U16)(4 + pcnt * 4);
        if (i + entry_len > size) break;
        i += entry_len;
        n++;
    }
    return n;
}

static int perform_flush(void)
{
    U16 actual_size = 0;
    U16 to_flush;
    int ret;
    LOG_BLOCK_HEADER_T header;
    const LOG_RAM_HEADER_T *ram_header;
    U16 usage = log_ram_get_usage();

    if (usage == 0 && !g_flush_force) return 0;

    to_flush = (usage > LOG_RAM_FLUSH_THRESHOLD) ? LOG_RAM_FLUSH_THRESHOLD : usage;
    if (to_flush > sizeof(g_flush_buffer))
        to_flush = sizeof(g_flush_buffer);

    ret = log_ram_read(g_flush_buffer, to_flush, &actual_size);
    if (ret != 0 || actual_size == 0) {
        printf("LOG_FLUSH: read from RAM failed\n");
        return -1;
    }

    ram_header = log_ram_get_header();
    ret = log_header_build(&header, actual_size,
                           count_log_entries(g_flush_buffer, actual_size),
                           ram_header->overflow_flag);
    if (ret != 0) {
        printf("LOG_FLUSH: header build failed\n");
        return -1;
    }

    {
        U32 part_off, part_size;
        U32 need = sizeof(header) + actual_size;
        if (log_storage_get_partition_info(&part_off, &part_size) != 0) return -1;
        if (need > part_size) return -1;
        if (g_storage_write_off + need > part_size)
            g_storage_write_off = 0;

        ret = log_storage_write(g_storage_write_off, (U8 *)&header, sizeof(header));
        if (ret != 0) { printf("LOG_FLUSH: header write failed\n"); return -1; }
        ret = log_storage_write(g_storage_write_off + sizeof(header),
                                g_flush_buffer, actual_size);
        if (ret != 0) { printf("LOG_FLUSH: data write failed\n"); return -1; }
        g_storage_write_off += need;
    }

    log_ram_clear_flushed(actual_size);
    g_last_flush_size = actual_size;

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_FLUSH: seq=%u wrote %u bytes (RAM now %u)\n",
           header.sequence, actual_size, log_ram_get_usage());
#endif
    return 0;
}

static void locate_storage_tail(void)
{
    U32 off = 0;
    U32 part_off, part_size;
    LOG_BLOCK_HEADER_T header;

    g_storage_write_off = 0;
    if (!log_storage_is_available()) return;
    if (log_storage_get_partition_info(&part_off, &part_size) != 0) return;
    while (off + sizeof(header) <= part_size) {
        if (log_storage_read(off, (U8 *)&header, sizeof(header)) != 0) break;
        if (!log_header_validate(&header)) break;
        off += sizeof(header) + header.data_size;
    }
    g_storage_write_off = off;
}

void log_flush_init(void)
{
    g_flush_status    = FLUSH_STATUS_IDLE;
    g_flush_force     = 0;
    g_total_flushes   = 0;
    g_failed_flushes  = 0;
    g_last_flush_size = 0;
    locate_storage_tail();
}

int log_flush_request(U8 force)
{
    if (g_flush_status == FLUSH_STATUS_IN_PROGRESS) return -1;
    g_flush_status = FLUSH_STATUS_PENDING;
    g_flush_force  = force;
    return 0;
}

int log_flush_process(void)
{
    int ret;
    if (g_flush_status != FLUSH_STATUS_PENDING) return 0;
    g_flush_status = FLUSH_STATUS_IN_PROGRESS;
    ret = perform_flush();
    if (ret == 0) {
        g_total_flushes++;
    } else {
        g_failed_flushes++;
        printf("LOG_FLUSH: flush failed (total failures: %u)\n", g_failed_flushes);
    }
    g_flush_status = FLUSH_STATUS_IDLE;
    g_flush_force  = 0;
    return ret;
}

FLUSH_STATUS_E log_flush_get_status(void) { return g_flush_status; }
U8             log_flush_is_busy(void)    { return (g_flush_status == FLUSH_STATUS_IN_PROGRESS) ? 1 : 0; }

int log_flush_now(void)
{
    if (log_flush_request(1) != 0) return -1;
    return log_flush_process();
}

void log_flush_get_stats(U32 *total_flushes, U32 *failed_flushes, U16 *last_flush_size)
{
    if (total_flushes)   *total_flushes   = g_total_flushes;
    if (failed_flushes)  *failed_flushes  = g_failed_flushes;
    if (last_flush_size) *last_flush_size = g_last_flush_size;
}

#endif /* WW_LOG_BACKEND_STORAGE */
