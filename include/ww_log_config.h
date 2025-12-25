/**
 * @file ww_log_config.h
 * @brief LOG module Phase2 configuration
 * @date 2025-12-24
 */

#ifndef WW_LOG_CONFIG_H
#define WW_LOG_CONFIG_H

#include "type.h"

/* ========== Simulation Mode Switch ========== */
/**
 * Define SIMULATION_MODE for PC simulation
 * Comment out for actual hardware
 */
#define SIMULATION_MODE

/* ========== Encode Mode Output Configuration ========== */

/**
 * WW_LOG_ENCODE_OUTPUT_TO_RAM
 * - 0: Output to UART (default, for debugging/decoding)
 * - 1: Output to RAM buffer (for production, requires WW_LOG_ENCODE_RAM_BUFFER_EN)
 *
 * Note: String mode always outputs to UART regardless of this setting
 */
#ifndef WW_LOG_ENCODE_OUTPUT_TO_RAM
#define WW_LOG_ENCODE_OUTPUT_TO_RAM  0
#endif

/* ========== RAM Configuration ========== */

#ifdef SIMULATION_MODE
    /* Simulation: Use static array */
    extern U8 g_sim_dlm_memory[4096];
    #define DLM_MAINTAIN_LOG_BASE_ADDR  ((U32)g_sim_dlm_memory)
    #define DLM_MAINTAIN_LOG_SIZE       4096
#else
    /* Actual hardware: Use linker script defined address */
    extern U8 __dlm_log_start;
    #define DLM_MAINTAIN_LOG_BASE_ADDR  ((U32)&__dlm_log_start)
    #define DLM_MAINTAIN_LOG_SIZE       4096
#endif

/* RAM buffer layout */
#define LOG_RAM_HEADER_SIZE         64      /* Header size in bytes */
#define LOG_RAM_DATA_SIZE           (DLM_MAINTAIN_LOG_SIZE - LOG_RAM_HEADER_SIZE)
#define LOG_RAM_FLUSH_THRESHOLD     3008    /* 3KB - trigger flush when reached */

/* ========== External Storage Configuration ========== */

#define LOG_STORAGE_PARTITION_SIZE  4096    /* 4KB LOG partition */
#define LOG_BLOCK_HEADER_SIZE       32      /* Block header size */

/* External memory type codes */
#define REG_WW_STUS_SYS_INFO_EXT_MEM_NONE   0
#define REG_WW_STUS_SYS_INFO_EXT_MEM_EEPROM 1
#define REG_WW_STUS_SYS_INFO_EXT_MEM_FLASH  2

/* Partition type codes */
#define PART_ENTRY_TYPE_LOG         5       /* LOG partition type */

/* ========== Magic Numbers ========== */

#define LOG_RAM_MAGIC               0x574C4F47  /* 'WLOG' */
#define LOG_BLOCK_MAGIC             0x4C4F4748  /* 'LOGH' */
#define LOG_RAM_VERSION             0x00020000  /* Phase2 v2.0.0 */

/* ========== Retry and Timeout ========== */

#define LOG_STORAGE_WRITE_RETRY     3       /* Retry times for storage write */
#define LOG_STORAGE_TIMEOUT_MS      100     /* Storage operation timeout */

/* ========== Debug Options ========== */

/* Enable detailed debug output */
// #define LOG_DEBUG_VERBOSE

/* Enable RAM buffer statistics */
#define LOG_RAM_STATISTICS

#endif /* WW_LOG_CONFIG_H */
