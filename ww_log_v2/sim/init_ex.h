/**
 * @file init_ex.h
 * @brief Sim stub for the firmware init_ex.h.
 *
 * In the real firmware this header (transitively) brings in the partition-table,
 * sys-info and device-lookup APIs that n_ww_log_storage.c uses. For the PC sim
 * we provide compatible declarations here; they are implemented (file/array
 * backed) in sim_ext_storage.c. Field names mirror the firmware so the log core
 * compiles unmodified.
 */

#ifndef __INIT_EX_H__
#define __INIT_EX_H__

#include "def.h"

/* ---- Partition table ---------------------------------------------------- */
#define PART_ENTRY_TYPE_LOG    (8)   /* TODO: VERIFY value matches fw enum */

typedef struct
{
    U8  part_type;
    U8  part_id;
    U8  slot_id;
    U8  reserved;
    U32 part_offset;
    U32 part_size;
} PART_ENTRY_T;

typedef struct
{
    U32 magic;
    U32 version;
    U32 product;
    U32 ptableSize;
    U16 pentryNum;
    U16 reserved;
    PART_ENTRY_T pentry[16];
} PART_TABLE_T;

PART_TABLE_T *pt_info_read(void);
WW_RTN        pt_table_check_valid(PART_TABLE_T *pt);
PART_ENTRY_T *pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2);

/* ---- Boot / system info register ---------------------------------------- */
typedef union
{
    struct
    {
        U32 bootMode   : 8;
        U32 extMemType : 8;   /* EXT_MEM_NONE / EXT_MEM_EEPROM / EXT_MEM_FLASH */
        U32 reserved   : 16;
    } sub;
    U32 raw;
} REG_WW_STUS_SYS_INFO_U;

REG_WW_STUS_SYS_INFO_U *reg_ww_stus_acc_sys_info_get(void);

/* ---- Device lookup ------------------------------------------------------- */
struct device;
const struct device *ww_get_device(const char *name);

#endif /* __INIT_EX_H__ */
