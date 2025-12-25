/**
 * @file ww_log_encode.c
 * @brief Encode mode logging implementation (variadic function version)
 * @date 2025-12-17
 */

#include "ww_log.h"
#include <stdio.h>
#include <stdarg.h>

#ifdef WW_LOG_MODE_ENCODE

/* Include RAM buffer header if RAM output is enabled */
#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
#include "ww_log_ram.h"
#endif

/* ========== Core Encoding Function ========== */

/**
 * @brief Core encode mode output function (variadic version)
 * @param module_id Module ID (0-31) for filtering
 * @param log_id File identifier (12 bits, 0-4095)
 * @param line Source line number
 * @param level Log level (0-3)
 * @param param_count Number of parameters (0-16)
 * @param ... Variable parameters (each as U32)
 *
 * This function performs all filtering internally to minimize code size
 * at each call site. Filtering checks:
 * 1. Module enable check (via g_ww_log_module_mask)
 * 2. Level threshold check (via g_ww_log_level_threshold)
 *
 * Output behavior controlled by WW_LOG_ENCODE_OUTPUT_TO_RAM:
 * - 0: Output to UART (hex format for debugging/decoding)
 * - 1: Output to RAM buffer (requires log_ram module)
 */
void ww_log_encode_output(U8 module_id, U16 log_id, U16 line, U8 level,
                U8 param_count, ...)
{
    U32 encoded_log;
    va_list args;
    U32 params[16];  /* Support up to 16 parameters */
    U8 i;

    /* Check module enable (dynamic switch) */
    if ((g_ww_log_module_mask & (1U << module_id)) == 0) {
        return;
    }

    /* Check level threshold (dynamic switch) */
    if (level > g_ww_log_level_threshold) {
        return;
    }

    /* Limit param_count for safety */
    if (param_count > 16) {
        param_count = 16;
    }

    /* Extract variadic parameters into array */
    if (param_count > 0) {
        va_start(args, param_count);
        for (i = 0; i < param_count; i++) {
            params[i] = va_arg(args, U32);
        }
        va_end(args);
    }

    /* Encode the log entry */
    encoded_log = WW_LOG_ENCODE(log_id, line, param_count, level);

#if (WW_LOG_ENCODE_OUTPUT_TO_RAM == 1)
    /* Output to RAM buffer */
    log_ram_write(encoded_log, params, param_count);
#else
    /* Output to UART as hex for debugging/decoding */
    /* Format: 0xHHHHHHHH 0xPPPPPPPP 0xPPPPPPPP ... */
    printf("0x%08X", encoded_log);

    /* Print all parameters */
    for (i = 0; i < param_count; i++) {
        printf(" 0x%08X", params[i]);
    }

    printf("\n");
    fflush(stdout);
#endif
}

#endif /* WW_LOG_MODE_ENCODE */
