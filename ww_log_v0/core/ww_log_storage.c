/**
 * @file ww_log_storage.c
 * @brief External storage abstraction layer implementation
 * @date 2025-12-24
 */

#include "ww_log_storage.h"
#include <string.h>
#include <stdio.h>

/* ========== Global Variables ========== */

static EXT_MEM_TYPE_E g_ext_mem_type = EXT_MEM_NONE;
static PART_TABLE_T *g_partition_table = NULL;
static PART_ENTRY_T *g_log_partition = NULL;
static U8 g_storage_initialized = 0;

/* ========== Private Functions ========== */

/**
 * @brief Find LOG partition in partition table
 */
static PART_ENTRY_T* find_log_partition(PART_TABLE_T *pt)
{
    if (pt == NULL) {
        return NULL;
    }

    /* Use partition table function to find LOG partition */
    return pt_entry_get_by_key(pt, PART_ENTRY_TYPE_LOG, 0, 0);
}

/* ========== Public Functions ========== */

int log_storage_init(void)
{
    /* Detect external memory type */
    g_ext_mem_type = log_storage_detect_type();

    if (g_ext_mem_type == EXT_MEM_NONE) {
#ifdef LOG_DEBUG_VERBOSE
        printf("LOG_STORAGE: No external memory detected\n");
#endif
        return -1;
    }

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_STORAGE: Detected type: %s\n",
           (g_ext_mem_type == EXT_MEM_EEPROM) ? "EEPROM" : "Flash");
#endif

    /* Read partition table */
    g_partition_table = log_storage_get_partition_table();
    if (g_partition_table == NULL) {
#ifdef LOG_DEBUG_VERBOSE
        printf("LOG_STORAGE: Failed to read partition table\n");
#endif
        return -1;
    }

    /* Validate partition table */
    if (!log_storage_check_partition_valid(g_partition_table)) {
#ifdef LOG_DEBUG_VERBOSE
        printf("LOG_STORAGE: Invalid partition table\n");
#endif
        return -1;
    }

    /* Find LOG partition */
    g_log_partition = find_log_partition(g_partition_table);
    if (g_log_partition == NULL) {
#ifdef LOG_DEBUG_VERBOSE
        printf("LOG_STORAGE: LOG partition not found\n");
#endif
        return -1;
    }

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_STORAGE: LOG partition found\n");
    printf("  Offset: 0x%08X\n", g_log_partition->part_offset);
    printf("  Size: %u bytes\n", g_log_partition->part_size);
    printf("  Type: %u\n", g_log_partition->part_type);
#endif

    g_storage_initialized = 1;
    return 0;
}

EXT_MEM_TYPE_E log_storage_detect_type(void)
{
#ifdef SIMULATION_MODE
    return (EXT_MEM_TYPE_E)g_sim_sys_info.extMemType;
#else
    /* Read from actual hardware register */
    return (EXT_MEM_TYPE_E)REG_WW_STUS_SYS_INFO_U.extMemType;
#endif
}

PART_TABLE_T* log_storage_get_partition_table(void)
{
    return pt_info_read();
}

U8 log_storage_check_partition_valid(PART_TABLE_T *pt)
{
    return pt_table_check_valid(pt);
}

PART_ENTRY_T* log_storage_get_log_partition(void)
{
    return g_log_partition;
}

int log_storage_write(U32 offset, const U8 *data, U32 size)
{
    int ret;
    U32 abs_offset;

    if (!g_storage_initialized) {
        return -1;
    }

    if (g_log_partition == NULL || data == NULL || size == 0) {
        return -1;
    }

    /* Check bounds */
    if (offset + size > g_log_partition->part_size) {
        return -1;
    }

    /* Calculate absolute offset */
    abs_offset = g_log_partition->part_offset + offset;

    /* Write based on storage type */
    if (g_ext_mem_type == EXT_MEM_EEPROM) {
        /* EEPROM: Direct write */
        ret = svc_eeprom_acc_write(abs_offset, data, size);
    } else if (g_ext_mem_type == EXT_MEM_FLASH) {
        /* Flash: Erase then write */
        /* Note: svc_flash_acc_write should handle erase internally */
        ret = svc_flash_acc_write(abs_offset, data, size);
    } else {
        return -1;
    }

    /* Retry on failure */
    if (ret != 0) {
        for (int i = 0; i < LOG_STORAGE_WRITE_RETRY; i++) {
            if (g_ext_mem_type == EXT_MEM_EEPROM) {
                ret = svc_eeprom_acc_write(abs_offset, data, size);
            } else {
                ret = svc_flash_acc_write(abs_offset, data, size);
            }
            if (ret == 0) {
                break;
            }
        }
    }

    return ret;
}

int log_storage_read(U32 offset, U8 *data, U32 size)
{
    U32 abs_offset;

    if (!g_storage_initialized) {
        return -1;
    }

    if (g_log_partition == NULL || data == NULL || size == 0) {
        return -1;
    }

    /* Check bounds */
    if (offset + size > g_log_partition->part_size) {
        return -1;
    }

    /* Calculate absolute offset */
    abs_offset = g_log_partition->part_offset + offset;

    /* Read based on storage type */
    if (g_ext_mem_type == EXT_MEM_EEPROM) {
        return svc_eeprom_acc_read(abs_offset, data, size);
    } else if (g_ext_mem_type == EXT_MEM_FLASH) {
        return svc_flash_acc_read(abs_offset, data, size);
    }

    return -1;
}

int log_storage_erase(U32 offset, U32 size)
{
    U32 abs_offset;

    if (!g_storage_initialized) {
        return -1;
    }

    if (g_log_partition == NULL) {
        return -1;
    }

    /* EEPROM doesn't need erase */
    if (g_ext_mem_type == EXT_MEM_EEPROM) {
        return 0;
    }

    /* Check bounds */
    if (offset + size > g_log_partition->part_size) {
        return -1;
    }

    /* Calculate absolute offset */
    abs_offset = g_log_partition->part_offset + offset;

    /* Flash erase */
    if (g_ext_mem_type == EXT_MEM_FLASH) {
#ifdef SIMULATION_MODE
        return sim_flash_erase(abs_offset, size);
#else
        /* Real hardware: Flash erase is typically handled by write function */
        /* If separate erase is needed, implement here */
        return 0;
#endif
    }

    return -1;
}

EXT_MEM_TYPE_E log_storage_get_current_type(void)
{
    return g_ext_mem_type;
}

int log_storage_get_partition_info(U32 *offset, U32 *size)
{
    if (!g_storage_initialized || g_log_partition == NULL) {
        return -1;
    }

    if (offset != NULL) {
        *offset = g_log_partition->part_offset;
    }

    if (size != NULL) {
        *size = g_log_partition->part_size;
    }

    return 0;
}

U8 log_storage_is_available(void)
{
    return (g_storage_initialized &&
            g_ext_mem_type != EXT_MEM_NONE &&
            g_log_partition != NULL) ? 1 : 0;
}
