/**
 * @file sim_storage.c
 * @brief Storage simulation implementation
 * @date 2025-12-24
 */

#include "sim_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>  /* For _mkdir on Windows */
#endif

/* ========== Global Variables ========== */

SIM_SYS_INFO_T g_sim_sys_info = {
    .extMemType = EXT_MEM_EEPROM,  /* Default to EEPROM */
};

PART_TABLE_T g_sim_partition_table = {0};
LAUNCH_INFO_T g_sim_launch_info = {0};

static FILE *g_eeprom_file = NULL;
static FILE *g_flash_file = NULL;
static U8 g_sim_initialized = 0;

/* ========== Private Functions ========== */

/**
 * @brief Create directory if it doesn't exist
 */
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

/**
 * @brief Initialize default partition table
 */
static void init_default_partition_table(void)
{
    memset(&g_sim_partition_table, 0, sizeof(PART_TABLE_T));

    g_sim_partition_table.magic = 0x50415254;  /* 'PART' */
    g_sim_partition_table.entry_count = 1;

    /* LOG partition */
    g_sim_partition_table.entries[0].part_offset = 0x1A00;  /* 6656 bytes */
    g_sim_partition_table.entries[0].part_size = 0x1000;    /* 4096 bytes */
    g_sim_partition_table.entries[0].part_type = PART_ENTRY_TYPE_LOG;
    g_sim_partition_table.entries[0].disk_type = (g_sim_sys_info.extMemType == EXT_MEM_EEPROM) ? 1 : 2;
    g_sim_partition_table.entries[0].part_id = 0;

    /* Copy to launch info */
    memcpy(&g_sim_launch_info.pt_info, &g_sim_partition_table, sizeof(PART_TABLE_T));
}

/* ========== Public Functions ========== */

int sim_storage_init(void)
{
    if (g_sim_initialized) {
        return 0;
    }

    printf("SIM_STORAGE: Initializing...\n");

    /* Create sim_data directory */
    create_directory("sim_data");

    /* Load configuration (simplified - use defaults for now) */
    /* In full implementation, this would load from JSON */
    init_default_partition_table();

    /* Create storage files */
    if (sim_create_storage_files() != 0) {
        printf("SIM_STORAGE: Failed to create storage files\n");
        return -1;
    }

    printf("SIM_STORAGE: Initialized successfully\n");
    printf("  Memory type: %s\n",
           (g_sim_sys_info.extMemType == EXT_MEM_EEPROM) ? "EEPROM" : "Flash");
    printf("  LOG partition: offset=0x%X, size=%u bytes\n",
           g_sim_partition_table.entries[0].part_offset,
           g_sim_partition_table.entries[0].part_size);

    g_sim_initialized = 1;
    return 0;
}

void sim_storage_cleanup(void)
{
    if (g_eeprom_file != NULL) {
        fclose(g_eeprom_file);
        g_eeprom_file = NULL;
    }

    if (g_flash_file != NULL) {
        fclose(g_flash_file);
        g_flash_file = NULL;
    }

    g_sim_initialized = 0;
}

int sim_create_storage_files(void)
{
    FILE *fp;
    U8 *buffer;

    /* Create EEPROM file if it doesn't exist */
    fp = fopen(SIM_EEPROM_FILE, "rb");
    if (fp == NULL) {
        printf("SIM_STORAGE: Creating EEPROM file...\n");
        fp = fopen(SIM_EEPROM_FILE, "wb");
        if (fp == NULL) {
            return -1;
        }

        /* Initialize with 0xFF */
        buffer = (U8*)malloc(SIM_EEPROM_SIZE);
        if (buffer == NULL) {
            fclose(fp);
            return -1;
        }
        memset(buffer, 0xFF, SIM_EEPROM_SIZE);
        fwrite(buffer, 1, SIM_EEPROM_SIZE, fp);
        free(buffer);
        fclose(fp);
    } else {
        fclose(fp);
    }

    /* Create Flash file if it doesn't exist */
    fp = fopen(SIM_FLASH_FILE, "rb");
    if (fp == NULL) {
        printf("SIM_STORAGE: Creating Flash file...\n");
        fp = fopen(SIM_FLASH_FILE, "wb");
        if (fp == NULL) {
            return -1;
        }

        /* Initialize with 0xFF */
        buffer = (U8*)malloc(SIM_FLASH_SIZE);
        if (buffer == NULL) {
            fclose(fp);
            return -1;
        }
        memset(buffer, 0xFF, SIM_FLASH_SIZE);
        fwrite(buffer, 1, SIM_FLASH_SIZE, fp);
        free(buffer);
        fclose(fp);
    } else {
        fclose(fp);
    }

    return 0;
}

/* ========== Partition Table Simulation ========== */

PART_TABLE_T* sim_pt_info_read(void)
{
    return &g_sim_partition_table;
}

PART_ENTRY_T* sim_pt_entry_get_by_key(PART_TABLE_T *pt, U8 type, U8 p1, U8 p2)
{
    if (pt == NULL) {
        return NULL;
    }

    /* Search for partition with matching type */
    for (U16 i = 0; i < pt->entry_count; i++) {
        if (pt->entries[i].part_type == type) {
            return &pt->entries[i];
        }
    }

    return NULL;
}

U8 sim_pt_table_check_valid(PART_TABLE_T *pt)
{
    if (pt == NULL) {
        return 0;
    }

    /* Check magic number */
    if (pt->magic != 0x50415254) {  /* 'PART' */
        return 0;
    }

    /* Check entry count */
    if (pt->entry_count == 0 || pt->entry_count > 16) {
        return 0;
    }

    return 1;
}

LAUNCH_INFO_T* sim_dlm_data_launch_info_get(void)
{
    return &g_sim_launch_info;
}

/* ========== EEPROM Simulation ========== */

int sim_eeprom_write(U32 offset, const U8 *data, U32 size)
{
    FILE *fp;

    if (data == NULL || size == 0) {
        return -1;
    }

    if (offset + size > SIM_EEPROM_SIZE) {
        printf("SIM_EEPROM: Write out of bounds (offset=0x%X, size=%u)\n", offset, size);
        return -1;
    }

    fp = fopen(SIM_EEPROM_FILE, "r+b");
    if (fp == NULL) {
        printf("SIM_EEPROM: Failed to open file for writing\n");
        return -1;
    }

    fseek(fp, offset, SEEK_SET);
    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) {
        printf("SIM_EEPROM: Write failed (expected=%u, actual=%zu)\n", size, written);
        return -1;
    }

#ifdef LOG_DEBUG_VERBOSE
    printf("SIM_EEPROM: Wrote %u bytes at offset 0x%X\n", size, offset);
#endif

    return 0;
}

int sim_eeprom_read(U32 offset, U8 *data, U32 size)
{
    FILE *fp;

    if (data == NULL || size == 0) {
        return -1;
    }

    if (offset + size > SIM_EEPROM_SIZE) {
        printf("SIM_EEPROM: Read out of bounds (offset=0x%X, size=%u)\n", offset, size);
        return -1;
    }

    fp = fopen(SIM_EEPROM_FILE, "rb");
    if (fp == NULL) {
        printf("SIM_EEPROM: Failed to open file for reading\n");
        return -1;
    }

    fseek(fp, offset, SEEK_SET);
    size_t read_size = fread(data, 1, size, fp);
    fclose(fp);

    if (read_size != size) {
        printf("SIM_EEPROM: Read failed (expected=%u, actual=%zu)\n", size, read_size);
        return -1;
    }

#ifdef LOG_DEBUG_VERBOSE
    printf("SIM_EEPROM: Read %u bytes from offset 0x%X\n", size, offset);
#endif

    return 0;
}

/* ========== Flash Simulation ========== */

int sim_flash_write(U32 offset, const U8 *data, U32 size)
{
    FILE *fp;

    if (data == NULL || size == 0) {
        return -1;
    }

    if (offset + size > SIM_FLASH_SIZE) {
        printf("SIM_FLASH: Write out of bounds (offset=0x%X, size=%u)\n", offset, size);
        return -1;
    }

    /* Flash write: erase first (set to 0xFF), then write */
    /* Simulate 4KB erase block */
    U32 erase_start = (offset / 4096) * 4096;
    U32 erase_size = ((offset + size + 4095) / 4096) * 4096 - erase_start;

    if (sim_flash_erase(erase_start, erase_size) != 0) {
        return -1;
    }

    fp = fopen(SIM_FLASH_FILE, "r+b");
    if (fp == NULL) {
        printf("SIM_FLASH: Failed to open file for writing\n");
        return -1;
    }

    fseek(fp, offset, SEEK_SET);
    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) {
        printf("SIM_FLASH: Write failed (expected=%u, actual=%zu)\n", size, written);
        return -1;
    }

#ifdef LOG_DEBUG_VERBOSE
    printf("SIM_FLASH: Wrote %u bytes at offset 0x%X (erased 0x%X-0x%X)\n",
           size, offset, erase_start, erase_start + erase_size);
#endif

    return 0;
}

int sim_flash_read(U32 offset, U8 *data, U32 size)
{
    FILE *fp;

    if (data == NULL || size == 0) {
        return -1;
    }

    if (offset + size > SIM_FLASH_SIZE) {
        printf("SIM_FLASH: Read out of bounds (offset=0x%X, size=%u)\n", offset, size);
        return -1;
    }

    fp = fopen(SIM_FLASH_FILE, "rb");
    if (fp == NULL) {
        printf("SIM_FLASH: Failed to open file for reading\n");
        return -1;
    }

    fseek(fp, offset, SEEK_SET);
    size_t read_size = fread(data, 1, size, fp);
    fclose(fp);

    if (read_size != size) {
        printf("SIM_FLASH: Read failed (expected=%u, actual=%zu)\n", size, read_size);
        return -1;
    }

#ifdef LOG_DEBUG_VERBOSE
    printf("SIM_FLASH: Read %u bytes from offset 0x%X\n", size, offset);
#endif

    return 0;
}

int sim_flash_erase(U32 offset, U32 size)
{
    FILE *fp;
    U8 *buffer;

    if (size == 0) {
        return 0;
    }

    if (offset + size > SIM_FLASH_SIZE) {
        printf("SIM_FLASH: Erase out of bounds (offset=0x%X, size=%u)\n", offset, size);
        return -1;
    }

    fp = fopen(SIM_FLASH_FILE, "r+b");
    if (fp == NULL) {
        printf("SIM_FLASH: Failed to open file for erasing\n");
        return -1;
    }

    /* Erase: set to 0xFF */
    buffer = (U8*)malloc(size);
    if (buffer == NULL) {
        fclose(fp);
        return -1;
    }
    memset(buffer, 0xFF, size);
    fseek(fp, offset, SEEK_SET);
    size_t written = fwrite(buffer, 1, size, fp);
    free(buffer);
    fclose(fp);

    if (written != size) {
        printf("SIM_FLASH: Erase failed (expected=%u, actual=%zu)\n", size, written);
        return -1;
    }

#ifdef LOG_DEBUG_VERBOSE
    printf("SIM_FLASH: Erased %u bytes at offset 0x%X\n", size, offset);
#endif

    return 0;
}

/* ========== Utility Functions ========== */

void sim_storage_dump(EXT_MEM_TYPE_E type, U32 offset, U32 size)
{
    U8 buffer[256];
    U32 to_read = (size > 256) ? 256 : size;
    int ret;

    printf("\n===== STORAGE DUMP =====\n");
    printf("Type: %s\n", (type == EXT_MEM_EEPROM) ? "EEPROM" : "Flash");
    printf("Offset: 0x%08X\n", offset);
    printf("Size: %u bytes\n", size);
    printf("------------------------\n");

    if (type == EXT_MEM_EEPROM) {
        ret = sim_eeprom_read(offset, buffer, to_read);
    } else {
        ret = sim_flash_read(offset, buffer, to_read);
    }

    if (ret != 0) {
        printf("Read failed\n");
        return;
    }

    for (U32 i = 0; i < to_read; i += 16) {
        printf("%04X: ", i);
        for (U32 j = 0; j < 16 && (i + j) < to_read; j++) {
            printf("%02X ", buffer[i + j]);
        }
        printf("\n");
    }

    printf("========================\n\n");
}
