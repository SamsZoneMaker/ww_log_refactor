/**
 * @file ww_log_storage.h
 * @brief External storage abstraction layer for LOG module Phase2
 * @date 2025-12-24
 *
 * This module provides a unified interface for external storage (EEPROM/Flash).
 * Features:
 * - Automatic storage type detection
 * - Partition table management
 * - Unified read/write interface
 * - Support for both EEPROM and Flash
 */

#ifndef WW_LOG_STORAGE_H
#define WW_LOG_STORAGE_H

#include "type.h"
#include "ww_log_config.h"

/* ========== External Memory Type ========== */

/**
 * External memory type enumeration
 */
typedef enum {
    EXT_MEM_NONE = REG_WW_STUS_SYS_INFO_EXT_MEM_NONE,      /* 0: No external memory */
    EXT_MEM_EEPROM = REG_WW_STUS_SYS_INFO_EXT_MEM_EEPROM,  /* 1: EEPROM */
    EXT_MEM_FLASH = REG_WW_STUS_SYS_INFO_EXT_MEM_FLASH     /* 2: Flash */
} EXT_MEM_TYPE_E;

/* ========== Partition Table Structures ========== */

/**
 * Partition entry structure (simplified version)
 * Compatible with server project's partition table
 */
typedef struct {
    U32 part_offset;        /**< Partition start address */
    U32 part_size;          /**< Partition size in bytes */
    U8  part_type;          /**< Partition type (LOG=5) */
    U8  disk_type;          /**< Disk type (EEPROM/Flash) */
    U8  part_id;            /**< Partition ID */
    U8  reserved;           /**< Reserved */
} PART_ENTRY_T;

/**
 * Partition table structure (simplified version)
 */
typedef struct {
    U32 magic;              /**< Magic number for validation */
    U16 entry_count;        /**< Number of partition entries */
    U16 reserved;           /**< Reserved */
    PART_ENTRY_T entries[16]; /**< Partition entries (max 16) */
} PART_TABLE_T;

/**
 * Launch info structure (for simulation)
 */
typedef struct {PART_TABLE_T pt_info;   /**< Partition table */
    /* Other fields... */
} LAUNCH_INFO_T;

/* ========== Simulation Mode Support ========== */

#ifdef SIMULATION_MODE

/**
 * System info register simulation
 */
typedef struct {
    U8 extMemType;          /**< External memory type */
    U8 reserved[3];         /**< Reserved */
} SIM_SYS_INFO_T;

extern SIM_SYS_INFO_T g_sim_sys_info;

/* Redefine register access for simulation */
#define REG_WW_STUS_SYS_INFO_U g_sim_sys_info

/* Simulation function declarations */
PART_TABLE_T* sim_pt_info_read(void);
PART_ENTRY_T* sim_pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2);
U8 sim_pt_table_check_valid(PART_TABLE_T *pt);
LAUNCH_INFO_T* sim_dlm_data_launch_info_get(void);

/* Redefine partition table functions for simulation */
#define pt_info_read()  sim_pt_info_read()
#define pt_entry_get_by_key  sim_pt_entry_get_by_key
#define pt_table_check_valid  sim_pt_table_check_valid
#define dlm_data_launch_info_get  sim_dlm_data_launch_info_get

/* Simulation storage functions */
int sim_eeprom_write(U32 offset, const U8 *data, U32 size);
int sim_eeprom_read(U32 offset, U8 *data, U32 size);
int sim_flash_write(U32 offset, const U8 *data, U32 size);
int sim_flash_read(U32 offset, U8 *data, U32 size);
int sim_flash_erase(U32 offset, U32 size);

/* Redefine SVC functions for simulation */
#define svc_eeprom_acc_write  sim_eeprom_write
#define svc_eeprom_acc_read   sim_eeprom_read
#define svc_flash_acc_write   sim_flash_write
#define svc_flash_acc_read    sim_flash_read

#else

/* Real hardware - include actual headers */
/* These would be provided by the server project */
/*
#include "init_ex.h"
#include "apiredun_ex.h"
#include "svc_ex.h"
*/

/* For now, declare prototypes for compilation */
PART_TABLE_T* pt_info_read(void);
PART_ENTRY_T* pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2);
U8 pt_table_check_valid(PART_TABLE_T *pt);
LAUNCH_INFO_T* dlm_data_launch_info_get(void);

int svc_eeprom_acc_write(U32 offset, const U8 *data, U32 size);
int svc_eeprom_acc_read(U32 offset, U8 *data, U32 size);
int svc_flash_acc_write(U32 vOfst, const U8 *data, U32 size);
int svc_flash_acc_read(U32 vOfst, U8 *data, U32 size);

#endif /* SIMULATION_MODE */

/* ========== Public Functions ========== */

/**
 * @brief Initialize storage module
 *
 * This function should be called after log_ram_init().
 * It will:
 * - Detect external memory type
 * - Read and validate partition table
 * - Find LOG partition
 *
 * @return 0=success, -1=error
 */
int log_storage_init(void);

/**
 * @brief Detect external memory type
 *
 * Reads from REG_WW_STUS_SYS_INFO_U.extMemType register
 *
 * @return External memory type
 */
EXT_MEM_TYPE_E log_storage_detect_type(void);

/**
 * @brief Get partition table
 *
 * @return Pointer to partition table, NULL if error
 */
PART_TABLE_T* log_storage_get_partition_table(void);

/**
 * @brief Validate partition table
 *
 * @param pt Pointer to partition table
 * @return 1=valid, 0=invalid
 */
U8 log_storage_check_partition_valid(PART_TABLE_T *pt);

/**
 * @brief Get LOG partition entry
 *
 * Searches for partition with type=PART_ENTRY_TYPE_LOG
 *
 * @return Pointer to LOG partition entry, NULL if not found
 */
PART_ENTRY_T* log_storage_get_log_partition(void);

/**
 * @brief Write data to external storage (unified interface)
 *
 * This function automatically handles EEPROM or Flash based on detected type.
 * For Flash, it will erase before write if needed.
 *
 * @param offset Offset relative to LOG partition start
 * @param data Data buffer to write
 * @param size Data size in bytes
 *
 * @return 0=success, <0=error
 */
int log_storage_write(U32 offset, const U8 *data, U32 size);

/**
 * @brief Read data from external storage (unified interface)
 *
 * @param offset Offset relative to LOG partition start
 * @param data Data buffer to read into
 * @param size Data size in bytes
 *
 * @return 0=success, <0=error
 */
int log_storage_read(U32 offset, U8 *data, U32 size);

/**
 * @brief Erase external storage (Flash only)
 *
 * For EEPROM, this is a no-op.
 * For Flash, erases the specified region.
 *
 * @param offset Offset relative to LOG partition start
 * @param size Size to erase in bytes
 *
 * @return 0=success, <0=error
 */
int log_storage_erase(U32 offset, U32 size);

/**
 * @brief Get current external memory type
 *
 * @return Current memory type
 */
EXT_MEM_TYPE_E log_storage_get_current_type(void);

/**
 * @brief Get LOG partition info
 *
 * @param offset [out] Partition offset
 * @param size [out] Partition size
 *
 * @return 0=success, -1=error
 */
int log_storage_get_partition_info(U32 *offset, U32 *size);

/**
 * @brief Check if storage is available
 *
 * @return 1=available, 0=not available
 */
U8 log_storage_is_available(void);

#endif /* WW_LOG_STORAGE_H */
