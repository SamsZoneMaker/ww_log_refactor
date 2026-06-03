/**
 * @file ww_log_panic.h
 * @brief Panic-mode logging and host-side dump helpers (CLAUDE.md §6).
 *
 * Call ww_log_panic() from a HardFault / watchdog handler. Semantics:
 *   1. bypass all module/level filtering;
 *   2. synchronously flush RAM -> external storage immediately (no threshold);
 *   3. switch UART to polling output (interrupt-independent);
 *   4. set a panic flag so subsequent LOG calls write through synchronously.
 *
 * The dump helpers (ww_log_ram_dump_file / ww_log_storage_dump_file) are
 * simulation-only. On real hardware the same data is pulled via JTAG; these
 * functions let the PC sim reproduce that read so the encode->dump->decode
 * loop can be verified end-to-end without hardware.
 */

#ifndef WW_LOG_PANIC_H
#define WW_LOG_PANIC_H

#include "type.h"
#include "ww_log_config.h"

/**
 * @brief Global panic flag.
 *
 * Checked by the output functions: when set, filtering is bypassed and entries
 * are emitted synchronously. Read-only for callers other than ww_log_panic().
 */
extern U8 g_ww_log_panic_flag;

/** @brief Enter panic mode and force-flush surviving logs. */
void ww_log_panic(void);

/** @brief 1 if panic mode is active. */
U8 ww_log_is_panic(void);

/* ============================================================
 * Dump helpers (SIMULATION_MODE only)
 * ============================================================ */

typedef enum {
    WW_LOG_DUMP_BIN = 0,   /**< raw binary snapshot */
    WW_LOG_DUMP_HEX = 1    /**< hex text frames ("0x.. 0x.."), usable as .txt */
} WW_LOG_DUMP_FMT_E;

#ifdef SIMULATION_MODE

#if (WW_LOG_BACKEND_RAM == 1)
/**
 * @brief Dump the RAM maintain region to a file.
 * @param path output file
 * @param fmt  WW_LOG_DUMP_BIN (full 4 KB region) or WW_LOG_DUMP_HEX (entries)
 * @return 0 = ok, -1 = error
 */
int ww_log_ram_dump_file(const char *path, WW_LOG_DUMP_FMT_E fmt);
#endif

#if (WW_LOG_BACKEND_STORAGE == 1)
/**
 * @brief Dump the external-storage LOG partition to a file.
 * @param path output file
 * @param fmt  WW_LOG_DUMP_BIN (partition snapshot) or WW_LOG_DUMP_HEX
 * @return 0 = ok, -1 = error
 */
int ww_log_storage_dump_file(const char *path, WW_LOG_DUMP_FMT_E fmt);
#endif

#endif /* SIMULATION_MODE */

#endif /* WW_LOG_PANIC_H */
