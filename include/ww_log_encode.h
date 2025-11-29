/**
 * @file ww_log_encode.h
 * @brief Encode mode logging implementation
 * @date 2025-11-29
 *
 * Encode mode features:
 * - Binary encoding for minimal code size
 * - No format strings stored in ROM
 * - Encoding format: 32-bit header
 *
 * Encoding Layout (32 bits):
 * ┌─────────────┬─────────────┬─────────┬─────────┐
 * │ LOG_ID      │ LINE        │ LEVEL   │ MSG_ID  │
 * │ (12 bits)   │ (12 bits)   │ (4 bits)│ (4 bits)│
 * │ 31       20 │ 19        8 │ 7     4 │ 3     0 │
 * └─────────────┴─────────────┴─────────┴─────────┘
 *
 * Where:
 * - LOG_ID: Module base ID + file offset (from CURRENT_LOG_ID)
 * - LINE: Source line number (__LINE__)
 * - LEVEL: Log level (ERR=0, WRN=1, INF=2, DBG=3)
 * - MSG_ID: Message identifier (auto-incremented per file)
 *
 * Usage in module internal header (e.g., brom_in.h):
 *   #define CURRENT_MODULE_BASE   WW_LOG_MOD_BROM_BASE
 *   #ifndef CURRENT_FILE_OFFSET
 *   #define CURRENT_FILE_OFFSET   0
 *   #endif
 *   #define CURRENT_LOG_ID        (CURRENT_MODULE_BASE + CURRENT_FILE_OFFSET)
 *
 * Usage in source file:
 *   // Normal file (uses default offset 0)
 *   #include "brom_in.h"
 *   LOG_INF();  // Encoded as: LOG_ID=160, LINE=__LINE__, LEVEL=2
 *
 *   // File with custom offset
 *   #define CURRENT_FILE_OFFSET  BROM_FILE_FLASH  // Defined in brom_in.h
 *   #include "brom_in.h"
 *   LOG_INF();  // Encoded as: LOG_ID=163, LINE=__LINE__, LEVEL=2
 */

#ifndef WW_LOG_ENCODE_H
#define WW_LOG_ENCODE_H

#include "file_id.h"

/* ========== Encoding Macros ========== */

/**
 * Encode a log entry into 32-bit format
 * @param log_id Module/file identifier (12 bits, 0-4095)
 * @param line Line number (12 bits, 0-4095)
 * @param level Log level (4 bits, 0-15)
 * @param msg_id Message ID (4 bits, 0-15)
 * @return 32-bit encoded value
 */
#define WW_LOG_ENCODE(log_id, line, level, msg_id) \
    ((U32)( \
        (((U32)(log_id) & 0xFFF) << 20) | \
        (((U32)(line) & 0xFFF) << 8) | \
        (((U32)(level) & 0xF) << 4) | \
        ((U32)(msg_id) & 0xF) \
    ))

/**
 * Decode macros to extract fields from encoded log
 */
#define WW_LOG_DECODE_LOG_ID(encoded)   (((encoded) >> 20) & 0xFFF)
#define WW_LOG_DECODE_LINE(encoded)     (((encoded) >> 8) & 0xFFF)
#define WW_LOG_DECODE_LEVEL(encoded)    (((encoded) >> 4) & 0xF)
#define WW_LOG_DECODE_MSG_ID(encoded)   ((encoded) & 0xF)

/* ========== Output Function Declaration ========== */

/**
 * @brief Core encode mode output function
 * @param encoded_log 32-bit encoded log entry
 *
 * This function outputs the encoded log to the configured target:
 * - UART (real-time output)
 * - RAM buffer (circular buffer for history)
 * - External storage (optional)
 */
void ww_log_encode_output(U32 encoded_log);

/* ========== Message ID Counter ========== */

/**
 * Message ID counter for automatic differentiation
 * Each log call in a file increments this counter
 * This allows distinguishing multiple logs on the same line
 * (e.g., in loops or macros)
 *
 * Note: This is a simple sequential counter within each compilation unit
 * It resets for each .c file, which is acceptable since LOG_ID+LINE
 * already provides file-level differentiation
 */
#ifndef _WW_LOG_MSG_COUNTER
#define _WW_LOG_MSG_COUNTER 0
#endif

/**
 * Auto-increment message ID
 * This is a simplified approach; for production, consider using __COUNTER__
 * if available, or a more sophisticated scheme
 */
#define _WW_LOG_NEXT_MSG_ID() _WW_LOG_MSG_COUNTER

/* ========== Internal Macro Implementations ========== */

/**
 * Internal encode macro with level parameter
 * Assumes CURRENT_LOG_ID is defined in the module's internal header
 */
#define _LOG_ENCODE_INTERNAL(level) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            U32 _encoded = WW_LOG_ENCODE( \
                CURRENT_LOG_ID, \
                __LINE__, \
                level, \
                _WW_LOG_NEXT_MSG_ID() \
            ); \
            ww_log_encode_output(_encoded); \
        } \
    } while(0)

/* ========== Public API Macros ========== */

/**
 * Public logging macros for encode mode
 * Note: Parameters are ignored in encode mode (no format string, no args)
 * The function signature remains compatible with str mode for source compatibility
 */
#define LOG_ERR(...) _LOG_ENCODE_INTERNAL(WW_LOG_LEVEL_ERR)
#define LOG_WRN(...) _LOG_ENCODE_INTERNAL(WW_LOG_LEVEL_WRN)
#define LOG_INF(...) _LOG_ENCODE_INTERNAL(WW_LOG_LEVEL_INF)
#define LOG_DBG(...) _LOG_ENCODE_INTERNAL(WW_LOG_LEVEL_DBG)

/* ========== RAM Buffer (Optional) ========== */

/**
 * RAM buffer configuration
 * Enable this to store encoded logs in a circular buffer
 */
#ifdef WW_LOG_ENCODE_RAM_BUFFER_EN

#ifndef WW_LOG_RAM_BUFFER_SIZE
#define WW_LOG_RAM_BUFFER_SIZE  128  /* Number of 32-bit entries */
#endif

/**
 * Circular buffer structure
 */
typedef struct {
    U32 magic;                              /* Magic number for warm restart detection */
    U16 head;                               /* Read pointer */
    U16 tail;                               /* Write pointer */
    U32 entries[WW_LOG_RAM_BUFFER_SIZE];   /* Log data */
} WW_LOG_RAM_BUFFER_T;

/**
 * Magic number for warm restart detection
 * ASCII: "WLOG" = 0x574C4F47
 */
#define WW_LOG_RAM_MAGIC  0x574C4F47

/**
 * Global RAM buffer instance
 */
extern WW_LOG_RAM_BUFFER_T g_ww_log_ram_buffer;

/**
 * RAM buffer API
 */
U16 ww_log_ram_get_count(void);
void ww_log_ram_dump(void);
void ww_log_ram_clear(void);

#endif /* WW_LOG_ENCODE_RAM_BUFFER_EN */

#endif /* WW_LOG_ENCODE_H */
