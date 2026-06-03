/**
 * @file ww_log_panic.c
 * @brief Panic-mode implementation and host-side dump helpers (CLAUDE.md §6).
 *
 * The panic flag is always compiled; the flush path is conditional on
 * RAM+STORAGE backends. The dump helpers are simulation-only.
 */

#include "ww_log_panic.h"
#include "ww_log_config.h"
#include <stdio.h>

#if (WW_LOG_BACKEND_RAM == 1) || (WW_LOG_BACKEND_STORAGE == 1)
#include "ww_log_store.h"
#endif

/* ============================================================
 * Panic
 * ============================================================ */

U8 g_ww_log_panic_flag = 0;

void ww_log_panic(void)
{
    /* (1) bypass filtering + (4) write-through for subsequent logs */
    g_ww_log_panic_flag = 1;

    /* (3) switch UART to polling. On the target this disables the TX interrupt
     * and busy-waits the FIFO; on PC the printf path is already synchronous. */
    printf("\n*** WW_LOG PANIC: forcing log preservation ***\n");
    fflush(stdout);

    /* (2) immediate synchronous flush RAM -> external storage */
#if (WW_LOG_BACKEND_STORAGE == 1)
    log_flush_now();
#elif (WW_LOG_BACKEND_RAM == 1)
    printf("*** WW_LOG PANIC: %u bytes retained in RAM ***\n",
           (unsigned)log_ram_get_usage());
#endif

    fflush(stdout);
}

U8 ww_log_is_panic(void)
{
    return g_ww_log_panic_flag;
}

/* ============================================================
 * Dump helpers (SIMULATION_MODE only)
 * ============================================================ */

#ifdef SIMULATION_MODE

#if (WW_LOG_BACKEND_RAM == 1) || (WW_LOG_BACKEND_STORAGE == 1)
static void write_entries_hex(FILE *fp, const U8 *buf, U32 size)
{
    U32 i = 0;
    while (i + 4 <= size) {
        U32 hdr  = *(const U32 *)(buf + i);
        U8  pcnt = (U8)(hdr & 0x3F);
        U32 need = 4 + (U32)pcnt * 4;
        U8  k;
        if (hdr == 0xFFFFFFFF) break;
        if (i + need > size) break;
        fprintf(fp, "0x%08X", hdr);
        for (k = 0; k < pcnt; k++)
            fprintf(fp, " 0x%08X", *(const U32 *)(buf + i + 4 + (U32)k * 4));
        fprintf(fp, "\n");
        i += need;
    }
}
#endif

#if (WW_LOG_BACKEND_RAM == 1)
int ww_log_ram_dump_file(const char *path, WW_LOG_DUMP_FMT_E fmt)
{
    FILE *fp;
    if (path == NULL) return -1;

    fp = fopen(path, (fmt == WW_LOG_DUMP_BIN) ? "wb" : "w");
    if (fp == NULL) return -1;

    if (fmt == WW_LOG_DUMP_BIN) {
        fwrite((const void *)DLM_MAINTAIN_LOG_BASE_ADDR, 1,
               DLM_MAINTAIN_LOG_SIZE, fp);
    } else {
        static U8 tmp[LOG_RAM_DATA_SIZE];
        U16 n = 0;
        log_ram_read(tmp, sizeof(tmp), &n);
        write_entries_hex(fp, tmp, n);
    }

    fclose(fp);
    return 0;
}
#endif /* WW_LOG_BACKEND_RAM */

#if (WW_LOG_BACKEND_STORAGE == 1)
int ww_log_storage_dump_file(const char *path, WW_LOG_DUMP_FMT_E fmt)
{
    static U8 buf[LOG_STORAGE_PARTITION_SIZE];
    U32 part_off, part_size;
    FILE *fp;

    if (path == NULL) return -1;
    if (log_storage_get_partition_info(&part_off, &part_size) != 0) return -1;
    if (part_size > sizeof(buf)) part_size = sizeof(buf);
    if (log_storage_read(0, buf, part_size) != 0) return -1;

    fp = fopen(path, (fmt == WW_LOG_DUMP_BIN) ? "wb" : "w");
    if (fp == NULL) return -1;

    if (fmt == WW_LOG_DUMP_BIN) {
        fwrite(buf, 1, part_size, fp);
    } else {
        U32 off = 0;
        while (off + sizeof(LOG_BLOCK_HEADER_T) <= part_size) {
            LOG_BLOCK_HEADER_T *h = (LOG_BLOCK_HEADER_T *)(buf + off);
            if (!log_header_validate(h)) break;
            write_entries_hex(fp, buf + off + sizeof(*h), h->data_size);
            off += sizeof(*h) + h->data_size;
        }
    }

    fclose(fp);
    return 0;
}
#endif /* WW_LOG_BACKEND_STORAGE */

#endif /* SIMULATION_MODE */
