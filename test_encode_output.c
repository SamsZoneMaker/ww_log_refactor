/**
 * @file test_encode_output.c
 * @brief Test program for encode output modes (UART vs RAM)
 * @date 2025-12-24
 */

#include "ww_log.h"
#include "ww_log_config.h"
#include <stdio.h>

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
#include "ww_log_ram.h"
#endif

int main(void)
{
    printf("\n========================================\n");
    printf("  Encode Output Mode Test\n");
    printf("========================================\n");

#if defined(WW_LOG_MODE_ENCODE)
    printf("Mode: ENCODE\n");

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    printf("Output: RAM Buffer\n");
    printf("========================================\n\n");

    /* Initialize RAM buffer */
    log_ram_init(1);  /* Force clear */

    /* Test logging */
    printf("Writing logs to RAM buffer...\n");
    LOG_ERR("Error message");
    LOG_WRN("Warning with value: %d", 100);
    LOG_INF("Info: x=%d y=%d", 10, 20);
    LOG_DBG("Debug: a=%d b=%d c=%d", 1, 2, 3);

    /* Check RAM buffer status */
    printf("\nRAM Buffer Status:\n");
    printf("  Usage: %u bytes\n", log_ram_get_usage());
    printf("  Available: %u bytes\n", log_ram_get_available());
    const LOG_RAM_HEADER_T *header = log_ram_get_header();
    printf("  Write Index: %u\n", header->write_index);
    printf("  Read Index: %u\n", header->read_index);
    printf("  Total Written: %u\n", header->total_written);

    /* Dump RAM buffer */
    printf("\n");
    log_ram_dump_hex();

#else
    printf("Output: UART (hex format)\n");
    printf("========================================\n\n");

    /* Test logging */
    printf("Writing logs to UART...\n");
    LOG_ERR("Error message");
    LOG_WRN("Warning with value: %d", 100);
    LOG_INF("Info: x=%d y=%d", 10, 20);
    LOG_DBG("Debug: a=%d b=%d c=%d", 1, 2, 3);

    printf("\nLogs should appear above in hex format.\n");
#endif

#else
    printf("Error: Not in ENCODE mode!\n");
    printf("Please set WW_LOG_MODE_ENCODE in ww_log.h\n");
#endif

    printf("\n========================================\n");
    printf("  Test Complete\n");
    printf("========================================\n\n");

    return 0;
}
