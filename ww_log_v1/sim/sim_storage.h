/**
 * @file sim_storage.h
 * @brief PC simulation of external storage (EEPROM/Flash) using files.
 *
 * Provides the svc_* / partition-table services the STORAGE backend expects,
 * backed by files under sim_data/. Lets the RAM->storage flush path run on a PC
 * with no hardware. Ported from v0.
 */

#ifndef SIM_STORAGE_H
#define SIM_STORAGE_H

#include "type.h"
#include "ww_log_store.h"

#define SIM_EEPROM_FILE  "sim_data/eeprom.bin"
#define SIM_FLASH_FILE   "sim_data/flash.bin"

#define SIM_EEPROM_SIZE  (64 * 1024)
#define SIM_FLASH_SIZE   (256 * 1024)

extern SIM_SYS_INFO_T g_sim_sys_info;
extern PART_TABLE_T   g_sim_partition_table;
extern LAUNCH_INFO_T  g_sim_launch_info;

int  sim_storage_init(void);
void sim_storage_cleanup(void);
int  sim_create_storage_files(void);

PART_TABLE_T  *sim_pt_info_read(void);
PART_ENTRY_T  *sim_pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2);
U8             sim_pt_table_check_valid(PART_TABLE_T *pt);
LAUNCH_INFO_T *sim_dlm_data_launch_info_get(void);

int sim_eeprom_write(U32 offset, const U8 *data, U32 size);
int sim_eeprom_read(U32 offset, U8 *data, U32 size);
int sim_flash_write(U32 offset, const U8 *data, U32 size);
int sim_flash_read(U32 offset, U8 *data, U32 size);
int sim_flash_erase(U32 offset, U32 size);

void sim_storage_dump(EXT_MEM_TYPE_E type, U32 offset, U32 size);

#endif /* SIM_STORAGE_H */
