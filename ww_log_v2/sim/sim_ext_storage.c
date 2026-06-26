/**
 * @file sim_ext_storage.c
 * @brief PC-sim hardware backend for the external-storage log path.
 *
 * Provides the device-level symbols the real n_ww_log_storage.c calls when
 * CONFIG_N_LOG_BACKEND_EXT_MEM is on (flash/eeprom read/write/erase, the
 * partition table, the boot/ext-mem-type sys-info, device lookup), backed by a
 * static array. This is NOT a fork of the log logic -- only the hardware shims.
 *
 * When the EXT_MEM backend is off this file is empty.
 */

#include "autoconf.h"

#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM

#include "def.h"
#include "init_ex.h"
#include "drivers/flash.h"
#include "drivers/eeprom.h"
#include "sim_ext_storage.h"
#include <stdio.h>
#include <string.h>

/* Mirror of EXT_MEM_TYPE_E (n_ww_log_storage.h) to avoid include coupling. */
#define SIM_MEM_NONE    0
#define SIM_MEM_EEPROM  1
#define SIM_MEM_FLASH   2

/* Which external memory the simulated board reports. Override in autoconf.h. */
#ifndef SIM_EXT_MEM_TYPE
#define SIM_EXT_MEM_TYPE   SIM_MEM_EEPROM
#endif

#define SIM_PART_MAGIC     (0x50415254)   /* 'PART' */

/* ---- backing store ------------------------------------------------------- */
static U8  g_sim_ext[SIM_EXT_TOTAL_SIZE];
static int g_sim_ext_ready = 0;

static void sim_ext_lazy_init(void)
{
    if (!g_sim_ext_ready)
    {
        memset(g_sim_ext, 0xFF, sizeof(g_sim_ext));
        g_sim_ext_ready = 1;
    }
}

void sim_ext_reset(void)
{
    memset(g_sim_ext, 0xFF, sizeof(g_sim_ext));
    g_sim_ext_ready = 1;
}

int sim_ext_dump_partition(const char *path)
{
    FILE *fp;
    sim_ext_lazy_init();
    if (path == NULL)
    {
        return -1;
    }
    fp = fopen(path, "wb");
    if (fp == NULL)
    {
        return -1;
    }
    fwrite(&g_sim_ext[SIM_EXT_LOG_OFFSET], 1, SIM_EXT_LOG_SIZE, fp);
    fclose(fp);
    return 0;
}

/* ---- device handles ------------------------------------------------------ */
struct device
{
    int         kind;   /* SIM_MEM_EEPROM / SIM_MEM_FLASH */
    const char *name;
};

static const struct device g_dev_eeprom = { SIM_MEM_EEPROM, "EEPROM_1" };
static const struct device g_dev_flash  = { SIM_MEM_FLASH,  "SPINOR_WM" };

const struct device *ww_get_device(const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }
    if (strcmp(name, "EEPROM_1") == 0)
    {
        return &g_dev_eeprom;
    }
    if (strcmp(name, "SPINOR_WM") == 0)
    {
        return &g_dev_flash;
    }
    return NULL;
}

/* ---- sys info ------------------------------------------------------------ */
REG_WW_STUS_SYS_INFO_U *reg_ww_stus_acc_sys_info_get(void)
{
    static REG_WW_STUS_SYS_INFO_U info;
    info.sub.bootMode   = 0;
    info.sub.extMemType = SIM_EXT_MEM_TYPE;
    info.sub.reserved   = 0;
    return &info;
}

/* ---- partition table ----------------------------------------------------- */
PART_TABLE_T *pt_info_read(void)
{
    static PART_TABLE_T pt;
    memset(&pt, 0, sizeof(pt));
    pt.magic      = SIM_PART_MAGIC;
    pt.version    = 1;
    pt.product    = 0;
    pt.ptableSize = sizeof(pt);
    pt.pentryNum  = 1;
    pt.pentry[0].part_type   = PART_ENTRY_TYPE_LOG;
    pt.pentry[0].part_id     = 0;
    pt.pentry[0].slot_id     = 0;
    pt.pentry[0].part_offset = SIM_EXT_LOG_OFFSET;
    pt.pentry[0].part_size   = SIM_EXT_LOG_SIZE;
    return &pt;
}

WW_RTN pt_table_check_valid(PART_TABLE_T *pt)
{
    if (pt == NULL || pt->magic != SIM_PART_MAGIC)
    {
        return WW_ERR;
    }
    if (pt->pentryNum == 0 || pt->pentryNum > 16)
    {
        return WW_ERR;
    }
    return WW_OK;
}

PART_ENTRY_T *pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2)
{
    U16 i;
    (void)p1;
    (void)p2;
    if (pt == NULL)
    {
        return NULL;
    }
    for (i = 0; i < pt->pentryNum && i < 16; i++)
    {
        if (pt->pentry[i].part_type == type)
        {
            return &pt->pentry[i];
        }
    }
    return NULL;
}

/* ---- flash ---------------------------------------------------------------
 * NOR semantics: erase sets bytes to 0xFF; write only clears bits. The sim
 * keeps it simple (write overwrites) and relies on the caller erasing first,
 * which the log code does for the whole LOG partition at init.            */
static int sim_ext_rw_ok(U32 offset, U32 len)
{
    return (len > 0) && (offset + len <= SIM_EXT_TOTAL_SIZE);
}

int flash_erase(const struct device *dev, U32 offset, U32 size)
{
    (void)dev;
    sim_ext_lazy_init();
    if (!sim_ext_rw_ok(offset, size))
    {
        return WW_ERR;
    }
    memset(&g_sim_ext[offset], 0xFF, size);
    return WW_OK;
}

int flash_write(const struct device *dev, U32 offset, U8 *buf, U32 len)
{
    (void)dev;
    sim_ext_lazy_init();
    if (buf == NULL || !sim_ext_rw_ok(offset, len))
    {
        return WW_ERR;
    }
    memcpy(&g_sim_ext[offset], buf, len);
    return WW_OK;
}

int flash_read(const struct device *dev, U32 offset, U8 *buf, U32 len)
{
    (void)dev;
    sim_ext_lazy_init();
    if (buf == NULL || !sim_ext_rw_ok(offset, len))
    {
        return WW_ERR;
    }
    memcpy(buf, &g_sim_ext[offset], len);
    return WW_OK;
}

/* ---- eeprom -------------------------------------------------------------- */
int eeprom_write(const struct device *dev, U32 offset, U8 *buf, U32 len)
{
    (void)dev;
    sim_ext_lazy_init();
    /* Matches fw ww_eeprom_write: rejects NULL data / zero len. */
    if (buf == NULL || len == 0 || !sim_ext_rw_ok(offset, len))
    {
        return WW_ERR;
    }
    memcpy(&g_sim_ext[offset], buf, len);
    return WW_OK;
}

int eeprom_read(const struct device *dev, U32 offset, U8 *buf, U32 len)
{
    (void)dev;
    sim_ext_lazy_init();
    if (buf == NULL || !sim_ext_rw_ok(offset, len))
    {
        return WW_ERR;
    }
    memcpy(buf, &g_sim_ext[offset], len);
    return WW_OK;
}

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */
