/**
 * @file ww_log_ram.c
 * @brief RAM buffer management implementation
 * @date 2025-12-24
 */

#include "ww_log_ram.h"
#include <string.h>
#include <stdio.h>

/* ========== Simulation Mode Support ========== */

#ifdef SIMULATION_MODE
/* Simulation: Use static array for DLM memory */
U8 g_sim_dlm_memory[4096] = {0};
#endif

/* ========== Global Variables ========== */

static LOG_RAM_BUFFER_T g_ram_buffer = {0};

#ifdef LOG_RAM_STATISTICS
static LOG_RAM_STATS_T g_ram_stats = {0};
#endif

/* ========== Private Functions ========== */

/**
 * @brief Calculate checksum for header (first 60 bytes)
 */
U32 log_ram_calc_checksum(const LOG_RAM_HEADER_T *header)
{
    U32 checksum = 0;
    const U8 *data = (const U8*)header;

    /* Calculate checksum for first 60 bytes (exclude checksum field itself) */
    for (U16 i = 0; i < (sizeof(LOG_RAM_HEADER_T) - sizeof(U32)); i++) {
        checksum += data[i];
    }

    return checksum;
}

/**
 * @brief Validate header integrity
 */
static U8 validate_header(const LOG_RAM_HEADER_T *header)
{
    /* Check magic number */
    if (header->magic != LOG_RAM_MAGIC) {
        return 0;
    }

    /* Check version */
    if (header->version != LOG_RAM_VERSION) {
        return 0;
    }

    /* Check pointers are within valid range */
    if (header->write_index >= LOG_RAM_DATA_SIZE ||
        header->read_index >= LOG_RAM_DATA_SIZE) {
        return 0;
    }

    /* Check checksum */
    U32 calculated = log_ram_calc_checksum(header);
    if (calculated != header->checksum) {
        return 0;
    }

    return 1;
}

/**
 * @brief Initialize header with default values
 */
static void init_header(LOG_RAM_HEADER_T *header)
{
    memset(header, 0, sizeof(LOG_RAM_HEADER_T));

    header->magic = LOG_RAM_MAGIC;
    header->version = LOG_RAM_VERSION;
    header->write_index = 0;
    header->read_index = 0;
    header->total_written = 0;
    header->flush_count = 0;
    header->last_flush_time = 0;
    header->overflow_flag = 0;

    /* Calculate and store checksum */
    header->checksum = log_ram_calc_checksum(header);
}

/**
 * @brief Update header checksum
 */
static void update_checksum(LOG_RAM_HEADER_T *header)
{
    header->checksum = log_ram_calc_checksum(header);
}

/**
 * @brief Get current usage (considering ring buffer wrap)
 */
static U16 get_current_usage(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;

    if (header->write_index >= header->read_index) {
        return header->write_index - header->read_index;
    } else {
        /* Wrapped around */
        return g_ram_buffer.data_size - header->read_index + header->write_index;
    }
}

/* ========== Public Functions ========== */

void log_ram_init(U8 force_clear)
{
    /* Get pointers to DLM memory */
    g_ram_buffer.header = (LOG_RAM_HEADER_T*)DLM_MAINTAIN_LOG_BASE_ADDR;
    g_ram_buffer.data = (U8*)(DLM_MAINTAIN_LOG_BASE_ADDR + LOG_RAM_HEADER_SIZE);
    g_ram_buffer.data_size = LOG_RAM_DATA_SIZE;
    g_ram_buffer.threshold = LOG_RAM_FLUSH_THRESHOLD;

    U8 valid = 0;

    if (!force_clear) {
        /* Try to preserve existing data */
        valid = validate_header(g_ram_buffer.header);

#ifdef LOG_DEBUG_VERBOSE
        if (valid) {
            printf("LOG_RAM: Valid header found, preserving data\n");
            printf("  Write: %u, Read: %u, Total: %u, Flushes: %u\n",
                   g_ram_buffer.header->write_index,
                   g_ram_buffer.header->read_index,
                   g_ram_buffer.header->total_written,
                   g_ram_buffer.header->flush_count);
        }
#endif
    }

    if (!valid || force_clear) {
        /* Initialize header */
        init_header(g_ram_buffer.header);

        /* Clear data area */
        memset(g_ram_buffer.data, 0, g_ram_buffer.data_size);

#ifdef LOG_DEBUG_VERBOSE
        printf("LOG_RAM: Initialized (force_clear=%u)\n", force_clear);
#endif
    }

#ifdef LOG_RAM_STATISTICS
    /* Reset statistics */
    memset(&g_ram_stats, 0, sizeof(g_ram_stats));
#endif
}

int log_ram_write(U32 encoded, U32 *params, U8 param_count)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 write_idx = header->write_index;
    U16 required = 4 + param_count * 4;  /* 4 bytes for encoded + 4*N for params */

    /* Check if we have enough space (considering wrap-around) */
    U16 available = log_ram_get_available();
    if (required > available) {
        /* Not enough space - need to wrap or flush */
        if (required > g_ram_buffer.data_size) {
            /* Entry too large for buffer */
            return -1;
        }

        /* Mark overflow and wrap to beginning */
        header->overflow_flag = 1;
        write_idx = 0;
        header->write_index = 0;

#ifdef LOG_RAM_STATISTICS
        g_ram_stats.overflow_count++;
#endif
    }

    /* Write encoded LOG entry */
    *(U32*)(g_ram_buffer.data + write_idx) = encoded;
    write_idx += 4;

    /* Write parameters */
    for (U8 i = 0; i < param_count; i++) {
        *(U32*)(g_ram_buffer.data + write_idx) = params[i];
        write_idx += 4;
    }

    /* Update write pointer */
    header->write_index = write_idx;
    header->total_written += required;

    /* Update checksum */
    update_checksum(header);

#ifdef LOG_RAM_STATISTICS
    g_ram_stats.write_calls++;
    g_ram_stats.write_bytes += required;
    U16 usage = get_current_usage();
    if (usage > g_ram_stats.peak_usage) {
        g_ram_stats.peak_usage = usage;
    }
#endif

    /* Check if flush is needed */
    if (log_ram_need_flush()) {
#ifdef LOG_RAM_STATISTICS
        g_ram_stats.flush_triggers++;
#endif
        return 1;  /* Need flush */
    }

    return 0;  /* Success */
}

U8 log_ram_need_flush(void)
{
    U16 usage = get_current_usage();
    return (usage >= g_ram_buffer.threshold) ? 1 : 0;
}

U16 log_ram_get_usage(void)
{
    return get_current_usage();
}

U16 log_ram_get_available(void)
{
    return g_ram_buffer.data_size - get_current_usage();
}

int log_ram_read(U8 *buffer, U16 max_size, U16 *actual_size)
{
    if (buffer == NULL || actual_size == NULL) {
        return -1;
    }

    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 read_idx = header->read_index;
    U16 write_idx = header->write_index;
    U16 copied = 0;

    /* Calculate available data */
    U16 available = get_current_usage();
    U16 to_copy = (available < max_size) ? available : max_size;

    if (to_copy == 0) {
        *actual_size = 0;
        return 0;
    }

    /* Handle ring buffer wrap-around */
    if (write_idx > read_idx) {
        /* Simple case: no wrap */
        memcpy(buffer, g_ram_buffer.data + read_idx, to_copy);
        copied = to_copy;
    } else {
        /* Wrapped: copy in two parts */
        U16 first_part = g_ram_buffer.data_size - read_idx;

        if (to_copy <= first_part) {
            /* All data in first part */
            memcpy(buffer, g_ram_buffer.data + read_idx, to_copy);
            copied = to_copy;
        } else {
            /* Need both parts */
            memcpy(buffer, g_ram_buffer.data + read_idx, first_part);
            U16 second_part = to_copy - first_part;
            memcpy(buffer + first_part, g_ram_buffer.data, second_part);
            copied = to_copy;
        }
    }

    *actual_size = copied;
    return 0;
}

void log_ram_clear_flushed(U16 size)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;

    /* Advance read pointer */
    header->read_index = (header->read_index + size) % g_ram_buffer.data_size;

    /* If read catches up with write, reset both to 0 for efficiency */
    if (header->read_index == header->write_index) {
        header->read_index = 0;
        header->write_index = 0;
        header->overflow_flag = 0;
    }

    /* Update flush count */
    header->flush_count++;

    /* Update checksum */
    update_checksum(header);
}

void log_ram_clear_all(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;

    /* Reset pointers */
    header->write_index = 0;
    header->read_index = 0;
    header->overflow_flag = 0;

    /* Update checksum */
    update_checksum(header);

    /* Clear data area */
    memset(g_ram_buffer.data, 0, g_ram_buffer.data_size);
}

const LOG_RAM_HEADER_T* log_ram_get_header(void)
{
    return g_ram_buffer.header;
}

#ifdef LOG_RAM_STATISTICS
void log_ram_get_stats(LOG_RAM_STATS_T *stats)
{
    if (stats != NULL) {
        memcpy(stats, &g_ram_stats, sizeof(LOG_RAM_STATS_T));
    }
}
#endif

void log_ram_dump_hex(void)
{
    LOG_RAM_HEADER_T *header = g_ram_buffer.header;
    U16 usage = get_current_usage();

    printf("\n===== RAM LOG BUFFER DUMP =====\n");
    printf("Magic: 0x%08X %s\n", header->magic,
           (header->magic == LOG_RAM_MAGIC) ? "(VALID)" : "(INVALID)");
    printf("Version: 0x%08X\n", header->version);
    printf("Write Index: %u\n", header->write_index);
    printf("Read Index: %u\n", header->read_index);
    printf("Usage: %u/%u bytes (%.1f%%)\n",
           usage, g_ram_buffer.data_size,
           (float)usage * 100.0f / g_ram_buffer.data_size);
    printf("Total Written: %u bytes\n", header->total_written);
    printf("Flush Count: %u\n", header->flush_count);
    printf("Overflow Flag: %u\n", header->overflow_flag);
    printf("Checksum: 0x%08X\n", header->checksum);
    printf("-------------------------------\n");

    if (usage > 0) {
        printf("Data (first %u bytes):\n", (usage > 256) ? 256 : usage);
        U16 read_idx = header->read_index;
        U16 to_print = (usage > 256) ? 256 : usage;

        for (U16 i = 0; i < to_print; i += 16) {
            printf("%04X: ", i);

            for (U16 j = 0; j < 16 && (i + j) < to_print; j++) {
                U16 idx = (read_idx + i + j) % g_ram_buffer.data_size;
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
