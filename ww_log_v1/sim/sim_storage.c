/**
 * @file sim_storage.c
 * @brief PC simulation of external storage (ported from v0).
 *
 * Gated on WW_LOG_BACKEND_STORAGE: only needed when the STORAGE backend is on.
 */

#include "ww_log_config.h"

#if (WW_LOG_BACKEND_STORAGE == 1)

#include "sim_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

/* ========== State ========== */

SIM_SYS_INFO_T g_sim_sys_info = { .extMemType = EXT_MEM_EEPROM };
PART_TABLE_T   g_sim_partition_table = {0};
LAUNCH_INFO_T  g_sim_launch_info = {0};

static U8 g_sim_initialized = 0;

/* ========== Private ========== */

static int create_directory(const char *path)
{
#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return _mkdir(path);
    }
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return mkdir(path, 0755);
    }
#endif
    return 0;
}

static void init_default_partition_table(void)
{
    memset(&g_sim_partition_table, 0, sizeof(PART_TABLE_T));
    g_sim_partition_table.magic = 0x50415254;   /* 'PART' */
    g_sim_partition_table.entry_count = 1;

    g_sim_partition_table.entries[0].part_offset = 0x1A00;
    g_sim_partition_table.entries[0].part_size = LOG_STORAGE_PARTITION_SIZE;
    g_sim_partition_table.entries[0].part_type = PART_ENTRY_TYPE_LOG;
    g_sim_partition_table.entries[0].disk_type =
        (g_sim_sys_info.extMemType == EXT_MEM_EEPROM) ? 1 : 2;
    g_sim_partition_table.entries[0].part_id = 0;

    memcpy(&g_sim_launch_info.pt_info, &g_sim_partition_table, sizeof(PART_TABLE_T));
}

/* ========== Public ========== */

int sim_storage_init(void)
{
    if (g_sim_initialized) {
        return 0;
    }
    create_directory("sim_data");
    init_default_partition_table();
    if (sim_create_storage_files() != 0) {
        printf("SIM_STORAGE: failed to create storage files\n");
        return -1;
    }
#ifdef LOG_DEBUG_VERBOSE
    printf("SIM_STORAGE: ready (%s, LOG part offset=0x%X size=%u)\n",
           (g_sim_sys_info.extMemType == EXT_MEM_EEPROM) ? "EEPROM" : "Flash",
           g_sim_partition_table.entries[0].part_offset,
           g_sim_partition_table.entries[0].part_size);
#endif
    g_sim_initialized = 1;
    return 0;
}

void sim_storage_cleanup(void)
{
    g_sim_initialized = 0;
}

static int create_file_if_absent(const char *name, U32 size)
{
    FILE *fp = fopen(name, "rb");
    if (fp != NULL) {
        fclose(fp);
        return 0;
    }
    fp = fopen(name, "wb");
    if (fp == NULL) {
        return -1;
    }
    U8 *buffer = (U8 *)malloc(size);
    if (buffer == NULL) {
        fclose(fp);
        return -1;
    }
    memset(buffer, 0xFF, size);
    fwrite(buffer, 1, size, fp);
    free(buffer);
    fclose(fp);
    return 0;
}

int sim_create_storage_files(void)
{
    if (create_file_if_absent(SIM_EEPROM_FILE, SIM_EEPROM_SIZE) != 0) {
        return -1;
    }
    if (create_file_if_absent(SIM_FLASH_FILE, SIM_FLASH_SIZE) != 0) {
        return -1;
    }
    return 0;
}

PART_TABLE_T *sim_pt_info_read(void)
{
    return &g_sim_partition_table;
}

PART_ENTRY_T *sim_pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2)
{
    U16 i;
    (void)p1;
    (void)p2;
    if (pt == NULL) {
        return NULL;
    }
    for (i = 0; i < pt->entry_count; i++) {
        if (pt->entries[i].part_type == type) {
            return &pt->entries[i];
        }
    }
    return NULL;
}

U8 sim_pt_table_check_valid(PART_TABLE_T *pt)
{
    if (pt == NULL || pt->magic != 0x50415254) {
        return 0;
    }
    if (pt->entry_count == 0 || pt->entry_count > 16) {
        return 0;
    }
    return 1;
}

LAUNCH_INFO_T *sim_dlm_data_launch_info_get(void)
{
    return &g_sim_launch_info;
}

/* ========== EEPROM ========== */

int sim_eeprom_write(U32 offset, const U8 *data, U32 size)
{
    FILE *fp;
    size_t written;

    if (data == NULL || size == 0 || offset + size > SIM_EEPROM_SIZE) {
        return -1;
    }
    fp = fopen(SIM_EEPROM_FILE, "r+b");
    if (fp == NULL) {
        return -1;
    }
    fseek(fp, offset, SEEK_SET);
    written = fwrite(data, 1, size, fp);
    fclose(fp);
    return (written == size) ? 0 : -1;
}

int sim_eeprom_read(U32 offset, U8 *data, U32 size)
{
    FILE *fp;
    size_t got;

    if (data == NULL || size == 0 || offset + size > SIM_EEPROM_SIZE) {
        return -1;
    }
    fp = fopen(SIM_EEPROM_FILE, "rb");
    if (fp == NULL) {
        return -1;
    }
    fseek(fp, offset, SEEK_SET);
    got = fread(data, 1, size, fp);
    fclose(fp);
    return (got == size) ? 0 : -1;
}

/* ========== Flash ========== */

int sim_flash_erase(U32 offset, U32 size)
{
    FILE *fp;
    U8 *buffer;
    size_t written;

    if (size == 0) {
        return 0;
    }
    if (offset + size > SIM_FLASH_SIZE) {
        return -1;
    }
    fp = fopen(SIM_FLASH_FILE, "r+b");
    if (fp == NULL) {
        return -1;
    }
    buffer = (U8 *)malloc(size);
    if (buffer == NULL) {
        fclose(fp);
        return -1;
    }
    memset(buffer, 0xFF, size);
    fseek(fp, offset, SEEK_SET);
    written = fwrite(buffer, 1, size, fp);
    free(buffer);
    fclose(fp);
    return (written == size) ? 0 : -1;
}

int sim_flash_write(U32 offset, const U8 *data, U32 size)
{
    FILE *fp;
    size_t written;
    U32 erase_start, erase_size;

    if (data == NULL || size == 0 || offset + size > SIM_FLASH_SIZE) {
        return -1;
    }
    /* Flash semantics: erase the touched 4KB blocks first. */
    erase_start = (offset / 4096) * 4096;
    erase_size = ((offset + size + 4095) / 4096) * 4096 - erase_start;
    if (sim_flash_erase(erase_start, erase_size) != 0) {
        return -1;
    }
    fp = fopen(SIM_FLASH_FILE, "r+b");
    if (fp == NULL) {
        return -1;
    }
    fseek(fp, offset, SEEK_SET);
    written = fwrite(data, 1, size, fp);
    fclose(fp);
    return (written == size) ? 0 : -1;
}

int sim_flash_read(U32 offset, U8 *data, U32 size)
{
    FILE *fp;
    size_t got;

    if (data == NULL || size == 0 || offset + size > SIM_FLASH_SIZE) {
        return -1;
    }
    fp = fopen(SIM_FLASH_FILE, "rb");
    if (fp == NULL) {
        return -1;
    }
    fseek(fp, offset, SEEK_SET);
    got = fread(data, 1, size, fp);
    fclose(fp);
    return (got == size) ? 0 : -1;
}

/* ========== Debug ========== */

void sim_storage_dump(EXT_MEM_TYPE_E type, U32 offset, U32 size)
{
    U8 buffer[256];
    U32 to_read = (size > 256) ? 256 : size;
    U32 i, j;
    int ret;

    printf("\n===== STORAGE DUMP (%s, offset=0x%08X, %u bytes) =====\n",
           (type == EXT_MEM_EEPROM) ? "EEPROM" : "Flash", offset, size);
    ret = (type == EXT_MEM_EEPROM)
        ? sim_eeprom_read(offset, buffer, to_read)
        : sim_flash_read(offset, buffer, to_read);
    if (ret != 0) {
        printf("read failed\n");
        return;
    }
    for (i = 0; i < to_read; i += 16) {
        printf("%04X: ", i);
        for (j = 0; j < 16 && (i + j) < to_read; j++) {
            printf("%02X ", buffer[i + j]);
        }
        printf("\n");
    }
    printf("===============================================\n\n");
}

#endif /* WW_LOG_BACKEND_STORAGE */
