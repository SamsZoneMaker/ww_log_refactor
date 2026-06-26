/**
 * @file autoconf.h
 * @brief Sim replacement for Kconfig-generated autoconf.h.
 *
 * Select the log mode by uncommenting exactly ONE of the three mode defines.
 * Backend enables can be toggled independently.
 *
 * IMPORTANT: The level constants (N_WW_LOG_LEVEL_*) are also defined here
 * to work around a circular-include ordering bug in n_ww_log_control.h vs
 * n_ww_log_output.h.  Without them here, only N_LOG_ERR compiles; WRN/INF/DBG
 * become no-ops.  See bug report for details.
 */

#ifndef __AUTOCONF_H__
#define __AUTOCONF_H__

/* ======================================================================
 * Log level constants
 * Duplicated here (also in n_ww_log_control.h) so that n_ww_log_output.h
 * can see them when it is included transitively from n_ww_log_control.h
 * before the control header has finished defining them.
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
#define CONFIG_N_LOG_MODE_STRING
// #define CONFIG_N_LOG_MODE_ENCODE

/* ======================================================================
 * Backend selection — can enable multiple
 * ====================================================================== */
#define CONFIG_N_LOG_BACKEND_UART     1
#define CONFIG_N_LOG_BACKEND_RAM      1
/* #define CONFIG_N_LOG_BACKEND_EXT_MEM  1 */   /* ext-mem requires real drivers */

#endif /* __AUTOCONF_H__ */
