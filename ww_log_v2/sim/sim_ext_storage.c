/**
 * @file sim_ext_storage.c
 * @brief PC-sim hardware backend for the external-storage log path.
 *
 * Provides the device-level symbols the real n_ww_log_storage.c calls when
 * CONFIG_N_LOG_BACKEND_EXT_MEM is on (flash/eeprom read/write/erase, the
 * partition table, and the boot/ext-mem-type sys-info), backed by files under
 * sim_data/. This is NOT a fork of the log logic -- only the hardware shims.
 *
 * When the EXT_MEM backend is off this file is empty.
 */

#include "autoconf.h"

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

/* Filled in when the ext block-ring backend is wired up. */

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */
