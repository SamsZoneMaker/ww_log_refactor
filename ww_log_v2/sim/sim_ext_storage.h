/**
 * @file sim_ext_storage.h
 * @brief Test helpers for the sim external-storage backend.
 *
 * The device-level API (flash/eeprom/pt/sysinfo) is declared in the firmware-
 * facing stubs (drivers/flash.h, drivers/eeprom.h, init_ex.h) and implemented
 * in sim_ext_storage.c. This header exposes only the extra hooks a host-side
 * test / main needs: dumping the LOG partition to a file for the decoder.
 */

#ifndef __SIM_EXT_STORAGE_H__
#define __SIM_EXT_STORAGE_H__

#include "def.h"

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

/* Geometry of the simulated external memory + LOG partition window. */
#define SIM_EXT_TOTAL_SIZE     (64 * 1024)
#define SIM_EXT_LOG_OFFSET     (0x2000)
#define SIM_EXT_LOG_SIZE       (4096)        /* 4K LOG partition (matches target) */

/* Wipe the backing store to 0xFF (erased) and reset the partition cache.
 * Call between tests for a clean external memory. */
void sim_ext_reset(void);

/* Write the LOG partition bytes (SIM_EXT_LOG_SIZE from SIM_EXT_LOG_OFFSET) to a
 * file, the way the host would pull them over JTAG, so log_decoder.py can run. */
int sim_ext_dump_partition(const char *path);

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */

#endif /* __SIM_EXT_STORAGE_H__ */
