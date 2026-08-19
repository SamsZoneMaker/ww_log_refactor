/**
 * @file main.c
 * @brief ww_log v1 simulation driver.
 *
 * Exercises the unified LOG API through demo modules, then demonstrates the
 * dynamic switches (module mask + level threshold). main.c itself is NOT
 * registered in log_config.json, so its own LOG_* calls (if any) are off by
 * design -- all visible logs come from the registered demo modules.
 *
 * Build mode is selected in include/ww_log.h (STR / ENCODE / DISABLED).
 */

#include "ww_log.h"
#include "ww_log_ctrl.h"
#include "ww_log_config.h"
#include <stdio.h>

#if (WW_LOG_BACKEND_RAM == 1) || (WW_LOG_BACKEND_STORAGE == 1)
#include "ww_log_store.h"
#endif
#ifdef SIMULATION_MODE
/* ---- Host-side dump helpers (sim only) -------------------------------------
 * On real hardware the same bytes are pulled via JTAG; here we read the
 * simulated DLM region / storage partition out to files so the
 * encode -> dump -> decode loop (log_decoder.py) can be verified end-to-end.
 * Relocated here from ww_log_panic.c after panic mode was removed. */
#if (WW_LOG_BACKEND_RAM == 1) || (WW_LOG_BACKEND_STORAGE == 1)

typedef enum {
    WW_LOG_DUMP_BIN = 0,   /* raw binary snapshot */
    WW_LOG_DUMP_HEX = 1    /* hex text frames ("0x.. 0x..") */
} WW_LOG_DUMP_FMT_E;

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

#if (WW_LOG_BACKEND_RAM == 1)
static int ww_log_ram_dump_file(const char *path, WW_LOG_DUMP_FMT_E fmt)
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
static int ww_log_storage_dump_file(const char *path, WW_LOG_DUMP_FMT_E fmt)
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

#endif /* RAM || STORAGE */
#endif /* SIMULATION_MODE */

/* DEMO module */
extern void demo_init(void);
extern void demo_process(int task_id);

/* DRIVERS module */
extern void drv_uart_init(void);
extern void drv_uart_send(int length);

/* TEST module (statically disabled) */
extern void test_unit_run(void);

static void banner(const char *title)
{
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

int main(void)
{
    printf("\n=======================================\n");
    printf("  ww_log v1 Simulation\n");
    printf("=======================================\n");
#if defined(WW_LOG_MODE_DISABLED)
    printf("  Mode: DISABLED\n");
#elif defined(WW_LOG_MODE_STR)
    printf("  Mode: STRING\n");
#elif defined(WW_LOG_MODE_ENCODE)
    printf("  Mode: ENCODE\n");
#endif
    printf("  Level threshold: %d (runtime configurable)\n",
           ww_log_get_level_threshold());
    printf("=======================================\n");

    ww_log_init();

    /* ===== Normal logging through registered modules ===== */
    banner("DEMO module");
    demo_init();
    demo_process(42);

    banner("DRIVERS module");
    drv_uart_init();
    drv_uart_send(128);

    banner("TEST module (statically disabled)");
    test_unit_run();

    /* ===== Dynamic switch: disable a module at runtime ===== */
    banner("Dynamic switch: disable DEMO (id=1)");
    ww_log_disable_module(1);
    printf("-- demo_init() below should produce NO demo logs --\n");
    demo_init();
    ww_log_enable_module(1);

    /* ===== Dynamic switch: raise level threshold to ERR ===== */
    banner("Dynamic switch: level threshold = ERR");
    ww_log_set_level_threshold(WW_LOG_LEVEL_ERR);
    printf("-- only ERR lines should appear below --\n");
    demo_process(-1);   /* hits LOG_ERR + early return */
    drv_uart_send(512); /* WRN/INF/DBG suppressed, none are ERR */
    ww_log_set_level_threshold(WW_LOG_LEVEL_DBG);

#if (WW_LOG_BACKEND_RAM == 1)
    banner("RAM backend status");
    printf("RAM usage: %u bytes, available: %u bytes\n",
           log_ram_get_usage(), log_ram_get_available());
#ifdef SIMULATION_MODE
    /* Dump the RAM region to files (host would do this over JTAG on target). */
    ww_log_ram_dump_file("ram_dump.bin", WW_LOG_DUMP_BIN);
    ww_log_ram_dump_file("ram_dump.hex", WW_LOG_DUMP_HEX);
    printf("RAM dumped -> ram_dump.bin / ram_dump.hex\n");
#endif
#if (WW_LOG_BACKEND_STORAGE == 1)
    printf("Forcing final flush...\n");
    log_flush_now();
    printf("RAM usage after flush: %u bytes\n", log_ram_get_usage());
#ifdef SIMULATION_MODE
    ww_log_storage_dump_file("storage_dump.bin", WW_LOG_DUMP_BIN);
    ww_log_storage_dump_file("storage_dump.hex", WW_LOG_DUMP_HEX);
    printf("Storage dumped -> storage_dump.bin / storage_dump.hex\n");
#endif
#endif
    log_ram_dump_hex();
#endif

    printf("\n=======================================\n");
    printf("  Done\n");
    printf("=======================================\n\n");
    return 0;
}
