/**
 * @file autoconf.h
 * @brief Sim replacement for the Kconfig-generated autoconf.h.
 *
 * Force-included on every translation unit by the Makefile (-include autoconf.h),
 * the same way the on-target Kconfig build injects these CONFIG_* symbols.
 * Select the log mode by uncommenting exactly ONE of the three mode defines.
 * Backend enables can be toggled independently.
 *
 * The level constants (N_WW_LOG_LEVEL_*) are also defined here to work around a
 * circular-include ordering issue in n_ww_log_control.h vs n_ww_log_output.h.
 */

#ifndef __AUTOCONF_H__
#define __AUTOCONF_H__

/* ======================================================================
 * Log level constants (also in n_ww_log_control.h; see note above)
 * ====================================================================== */
#ifndef N_WW_LOG_LEVEL_ERR
#define N_WW_LOG_LEVEL_ERR    0
#define N_WW_LOG_LEVEL_WRN    1
#define N_WW_LOG_LEVEL_INF    2
#define N_WW_LOG_LEVEL_DBG    3
#endif

#ifndef N_WW_LOG_COMPILE_THRESHOLD
#define N_WW_LOG_COMPILE_THRESHOLD    N_WW_LOG_LEVEL_DBG
#endif

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
