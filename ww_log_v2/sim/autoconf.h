/**
 * @file autoconf.h
 * @brief Sim replacement for the Kconfig-generated autoconf.h.
 *
 * Force-included on every translation unit by the Makefile (-include autoconf.h),
 * the same way the on-target Kconfig build injects these CONFIG_* symbols.
 * Select the log mode by uncommenting exactly ONE of the three mode defines.
 * Backend enables can be toggled independently.
 */

#ifndef __AUTOCONF_H__
#define __AUTOCONF_H__

/* ======================================================================
 * Log mode — uncomment exactly ONE
 * ====================================================================== */
// #define CONFIG_N_LOG_MODE_DISABLED
// #define CONFIG_N_LOG_MODE_STRING
#define CONFIG_N_LOG_MODE_ENCODE

/* ======================================================================
 * Backend selection — can enable multiple
 * ====================================================================== */
#define CONFIG_N_LOG_BACKEND_UART     1
#define CONFIG_N_LOG_BACKEND_RAM      1
#define CONFIG_N_LOG_BACKEND_EXT_MEM  1

/* External-storage full policy (exactly one; default FREEZE in n_ww_log_storage.h).
 * The ext log is an append-only stream, so there is no per-slot rolling window:
 *   FREEZE - stop appending once full (keep the earliest persisted logs)
 *   ERASE  - wipe the partition and restart (keep the newest logs) */
#define CONFIG_N_LOG_EXT_FULL_FREEZE
// #define CONFIG_N_LOG_EXT_FULL_ERASE

/* Which levels get persisted to external storage (RAM keeps all). Entries with
 * level <= this are copied on flush; default WRN (only ERR/WRN). Override to
 * N_WW_LOG_LEVEL_DBG to persist everything. */
// #define N_WW_LOG_EXT_LEVEL_THRESHOLD   N_WW_LOG_LEVEL_WRN

/* Prepend an 8-byte [marker][tick] record to the first data-bearing flush of
 * each flush-task wake, so the host decoder can split the append stream per
 * flush and spot reboots (tick resets). Comment out to save 8 bytes/flush. */
#define CONFIG_N_LOG_EXT_FLUSH_MARKER

#endif /* __AUTOCONF_H__ */
