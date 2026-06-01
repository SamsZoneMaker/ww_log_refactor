/**
 * @file sim_storage.h
 * @brief Storage simulation for PC testing
 * @date 2025-12-24
 *
 * This module simulates EEPROM/Flash storage using files.
 */

#ifndef SIM_STORAGE_H
#define SIM_STORAGE_H

#include "type.h"
#include "ww_log_storage.h"

/* ========== Simulation Configuration ========== */

#define SIM_EEPROM_FILE     "sim_data/eeprom.bin"
#define SIM_FLASH_FILE      "sim_data/flash.bin"
#define SIM_CONFIG_FILE     "sim/sim_config.json"

#define SIM_EEPROM_SIZE     (64 * 1024)  /* 64KB */
#define SIM_FLASH_SIZE      (256 * 1024) /* 256KB */

/* ========== Global Variables ========== */

extern SIM_SYS_INFO_T g_sim_sys_info;
extern PART_TABLE_T g_sim_partition_table;
extern LAUNCH_INFO_T g_sim_launch_info;

/* ========== Simulation Functions ========== */

/**
 * @brief Initialize simulation storage
 *
 * This function should be called before any storage operations.
 * It will:
 * - Load configuration from JSON
 * - Create/open storage files
 * - Initialize partition table
 *
 * @return 0=success, -1=error
 */
int sim_storage_init(void);

/**
 * @brief Cleanup simulation storage
 */
void sim_storage_cleanup(void);

/**
 * @brief Load configuration from JSON file
 *
 * @return 0=success, -1=error
 */
int sim_load_config(void);

/**
 * @brief Create storage files if they don't exist
 *
 * @return 0=success, -1=error
 */
int sim_create_storage_files(void);

/* ========== Partition Table Simulation ========== */

/**
 * @brief Get partition table (simulation)
 */
PART_TABLE_T* sim_pt_info_read(void);

/**
 * @brief Get partition entry by key (simulation)
 */
PART_ENTRY_T* sim_pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2);

/**
 * @brief Check partition table validity (simulation)
 */
U8 sim_pt_table_check_valid(PART_TABLE_T *pt);

/**
 * @brief Get launch info (simulation)
 */
LAUNCH_INFO_T* sim_dlm_data_launch_info_get(void);

/* ========== EEPROM Simulation ========== */

/**
 * @brief Write to EEPROM (simulation)
 *
 * @param offset Absolute offset in EEPROM
 * @param data Data to write
 * @param size Data size
 *
 * @return 0=success, -1=error
 */
int sim_eeprom_write(U32 offset, const U8 *data, U32 size);

/**
 * @brief Read from EEPROM (simulation)
 *
 * @param offset Absolute offset in EEPROM
 * @param data Buffer to read into
 * @param size Data size
 *
 * @return 0=success, -1=error
 */
int sim_eeprom_read(U32 offset, U8 *data, U32 size);

/* ========== Flash Simulation ========== */

/**
 * @brief Write to Flash (simulation)
 *
 * Note: This function will erase before write automatically
 *
 * @param offset Absolute offset in Flash
 * @param data Data to write
 * @param size Data size
 *
 * @return 0=success, -1=error
 */
int sim_flash_write(U32 offset, const U8 *data, U32 size);

/**
 * @brief Read from Flash (simulation)
 *
 * @param offset Absolute offset in Flash
 * @param data Buffer to read into
 * @param size Data size
 *
 * @return 0=success, -1=error
 */
int sim_flash_read(U32 offset, U8 *data, U32 size);

/**
 * @brief Erase Flash (simulation)
 *
 * Sets the specified region to 0xFF
 *
 * @param offset Absolute offset in Flash
 * @param size Size to erase
 *
 * @return 0=success, -1=error
 */
int sim_flash_erase(U32 offset, U32 size);

/* ========== Utility Functions ========== */

/**
 * @brief Dump storage content (for debugging)
 *
 * @param type Storage type
 * @param offset Offset to dump from
 * @param size Size to dump
 */
void sim_storage_dump(EXT_MEM_TYPE_E type, U32 offset, U32 size);

#endif /* SIM_STORAGE_H */
