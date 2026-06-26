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

/* External-storage full policy (exactly one; default RING in n_ww_log_storage.h).
 *   RING   - overwrite the oldest block (keep most recent, for crash forensics)
 *   FREEZE - stop flushing once full (keep earliest, e.g. boot logs) */
#define CONFIG_N_LOG_EXT_POLICY_RING
// #define CONFIG_N_LOG_EXT_POLICY_FREEZE

#endif /* __AUTOCONF_H__ */
