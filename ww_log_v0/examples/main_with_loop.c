/**
 * @file main_with_loop.c
 * @brief Example application with main loop and flush mechanism
 * @date 2026-01-05
 *
 * This example demonstrates how to properly use the LOG flush mechanism
 * in a real application with a main loop.
 *
 * Key points:
 * 1. Initialize flush mechanism with log_flush_init()
 * 2. Call log_flush_process() periodically in main loop
 * 3. Use log_flush_now() for immediate flush before critical events
 */

#include "ww_log.h"
#include <stdio.h>
#include <stdlib.h>

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
#include "ww_log_flush.h"
#include "ww_log_ram.h"
#endif

/* Simulation: delay function */
#ifdef _WIN32
#include <windows.h>
#define delay_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define delay_ms(ms) usleep((ms) * 1000)
#endif

/* Application state */
static int g_loop_count = 0;
static int g_task_count = 0;

/**
 * @brief Simulate application task processing
 */
static void app_process_tasks(void)
{
    g_task_count++;

    /* Simulate different types of tasks */
    if (g_task_count % 10 == 0) {
        LOG_INF("Task checkpoint: %d tasks processed", g_task_count);
    }

    if (g_task_count % 50 == 0) {
        LOG_WRN("Heavy task executed: task_id=%d", g_task_count);
    }

    if (g_task_count % 100 == 0) {
        LOG_ERR("Critical task: task_id=%d, status=OK", g_task_count);
    }
}

/**
 * @brief Main function with loop
 */
int main(void)
{
    printf("\n");
    printf("=======================================\n");
    printf("  LOG System - Main Loop Example\n");
    printf("=======================================\n");

#if defined(WW_LOG_MODE_ENCODE) && (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    printf("  Mode: ENCODE with RAM buffer\n");
    printf("  RAM Size: 4KB\n");
    printf("  Flush Threshold: 3KB\n");
#else
    printf("  Mode: STRING or ENCODE without RAM\n");
#endif
    printf("=======================================\n\n");

    /* Initialize log system */
    ww_log_init();

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    /* Initialize flush mechanism */
    log_flush_init();
    printf("LOG flush mechanism initialized\n\n");
#endif

    LOG_INF("Application started");
    LOG_INF("Entering main loop...");

    /* Main loop */
    printf("Starting main loop (will run 1000 iterations)...\n");
    printf("Press Ctrl+C to stop\n\n");

    while (g_loop_count < 1000) {
        g_loop_count++;

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
        /* CRITICAL: Process flush requests periodically */
        /* This is where the actual flush happens */
        log_flush_process();
#endif

        /* Application business logic */
        app_process_tasks();

        /* Show status every 100 iterations */
        if (g_loop_count % 100 == 0) {
            printf("[Loop %d] Tasks: %d", g_loop_count, g_task_count);

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
            printf(", RAM usage: %u bytes", log_ram_get_usage());

            U32 total_flushes, failed_flushes;
            U16 last_flush_size;
            log_flush_get_stats(&total_flushes, &failed_flushes, &last_flush_size);
            printf(", Flushes: %u", total_flushes);
#endif
            printf("\n");
        }

        /* Simulate 10ms loop period */
        delay_ms(10);
    }

    printf("\nMain loop completed\n");

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    /* Before exit, flush all remaining logs */
    printf("\nFlushing remaining logs before exit...\n");
    LOG_INF("Application shutting down");
    int flush_ret = log_flush_now();
    if (flush_ret == 0) {
        printf("Final flush completed successfully\n");
    } else {
        printf("Final flush failed: %d\n", flush_ret);
    }

    /* Show final statistics */
    printf("\n=== Final Statistics ===\n");
    printf("RAM usage: %u bytes\n", log_ram_get_usage());
    printf("RAM available: %u bytes\n", log_ram_get_available());
    U32 total_flushes, failed_flushes;
    U16 last_flush_size;
    log_flush_get_stats(&total_flushes, &failed_flushes, &last_flush_size);
    printf("Total flushes: %u\n", total_flushes);
    printf("Failed flushes: %u\n", failed_flushes);
    printf("Last flush size: %u bytes\n", last_flush_size);

    const LOG_RAM_HEADER_T *header = log_ram_get_header();
    printf("Total written: %u bytes\n", header->total_written);
    printf("Flush count: %u\n", header->flush_count);
    printf("Overflow flag: %u\n", header->overflow_flag);
#endif

    printf("\n=======================================\n");
    printf("  Application Exit\n");
    printf("=======================================\n\n");

    return 0;
}
