/**
 * @file ww_log_config.h
 * @brief ww_log v1 compile-time configuration
 *
 * Central knobs for:
 * - Simulation vs hardware
 * - Output backend selection (composable, see CLAUDE.md §5)
 * - RAM ring-buffer geometry / flush threshold
 * - Magic numbers and versions
 */

#ifndef WW_LOG_CONFIG_H
#define WW_LOG_CONFIG_H

#include "type.h"

/* ========== Simulation Mode Switch ========== */
/**
 * Define SIMULATION_MODE for PC simulation (gcc).
 * Comment out for real hardware (RISC-V Andes N25).
 */
#define SIMULATION_MODE

/* ========== Output Backends (composable, see CLAUDE.md §5) ========== */
/**
 * Each backend is an independent on/off switch and they may be combined.
 * The output path dispatches an encoded entry to every enabled backend.
 *
 *   WW_LOG_BACKEND_UART     hex frame (encode) / readable text (str)
 *   WW_LOG_BACKEND_RAM      4KB ring buffer in the maintain region
 *   WW_LOG_BACKEND_STORAGE  flush RAM -> external EEPROM/Flash
 *
 * Default: UART + RAM. encode logs are emitted on UART AND copied into the
 * power-loss retained RAM region (dump it later via ww_log_ram_dump_file / JTAG).
 * STORAGE stays opt-in (it touches external EEPROM/Flash): enable with
 * -DWW_LOG_BACKEND_STORAGE=1.
 */
#ifndef WW_LOG_BACKEND_UART
#define WW_LOG_BACKEND_UART     1
#endif
#ifndef WW_LOG_BACKEND_RAM
#define WW_LOG_BACKEND_RAM      1
#endif
#ifndef WW_LOG_BACKEND_STORAGE
#define WW_LOG_BACKEND_STORAGE  0
#endif

/* ========== RAM Configuration ========== */

#ifdef SIMULATION_MODE
    /* Simulation: use a static array as the maintain (no-power-loss) region.
     * Use uintptr_t (NOT U32): on a 64-bit PC, truncating the array address to
     * 32 bits yields an invalid pointer and segfaults on first access. On the
     * 32-bit target uintptr_t is 32 bits, so this is correct there too. */
    extern U8 g_sim_dlm_memory[4096];
    #define DLM_MAINTAIN_LOG_BASE_ADDR  ((uintptr_t)g_sim_dlm_memory)
    #define DLM_MAINTAIN_LOG_SIZE       4096
#else
    /* Real hardware: address provided by the linker script */
    extern U8 __dlm_log_start;
    #define DLM_MAINTAIN_LOG_BASE_ADDR  ((uintptr_t)&__dlm_log_start)
    #define DLM_MAINTAIN_LOG_SIZE       4096
#endif

/* RAM buffer layout: 64B header + data area, flush at 3KB */
#define LOG_RAM_HEADER_SIZE         64
#define LOG_RAM_DATA_SIZE           (DLM_MAINTAIN_LOG_SIZE - LOG_RAM_HEADER_SIZE)
#define LOG_RAM_FLUSH_THRESHOLD     3008

/* ========== External Storage Configuration ========== */

#define LOG_STORAGE_PARTITION_SIZE  4096
#define LOG_BLOCK_HEADER_SIZE       32

/* External memory type codes */
#define REG_WW_STUS_SYS_INFO_EXT_MEM_NONE   0
#define REG_WW_STUS_SYS_INFO_EXT_MEM_EEPROM 1
#define REG_WW_STUS_SYS_INFO_EXT_MEM_FLASH  2

/* Partition type codes */
#define PART_ENTRY_TYPE_LOG         5

/* ========== Magic Numbers ========== */

#define LOG_RAM_MAGIC               0x574C4F47  /* 'WLOG' */
#define LOG_BLOCK_MAGIC             0x4C4F4748  /* 'LOGH' */
#define LOG_RAM_VERSION             0x00030000  /* v1 (3.0.0) */

/* ========== Retry and Timeout ========== */

#define LOG_STORAGE_WRITE_RETRY     3
#define LOG_STORAGE_TIMEOUT_MS      100

/* ========== Debug Options ========== */

/* #define LOG_DEBUG_VERBOSE */
#define LOG_RAM_STATISTICS

#endif /* WW_LOG_CONFIG_H */
