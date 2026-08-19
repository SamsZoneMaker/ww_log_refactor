/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* ww_log shared definitions: log levels, compile-time thresholds, the encode
 * bit-field layout and per-file injected macros. This is the LOWEST layer of
 * the log module -- it depends on nothing but ww_type.h and is included by all
 * the other log headers, which is what keeps the include graph acyclic
 * (def <- output <- macro, def <- api; no header includes its dependents). */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_DEF_H__
#define __N_WW_LOG_DEF_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "ww_type.h"

/*************************** macro definition start ***************************/

/* ========= Log levels ========= */
#define N_WW_LOG_LEVEL_ERR    0  /* Error: failures, critical issues */
#define N_WW_LOG_LEVEL_WRN    1  /* Warning: potential problems */
#define N_WW_LOG_LEVEL_INF    2  /* Info: important state changes */
#define N_WW_LOG_LEVEL_DBG    3  /* Debug: detailed execution flow */

/**
 * Compile-time level threshold. Logs with level > threshold are compiled out
 * entirely (zero code size). Override via -DN_WW_LOG_COMPILE_THRESHOLD=n.
 */
#ifndef N_WW_LOG_COMPILE_THRESHOLD
#define N_WW_LOG_COMPILE_THRESHOLD    N_WW_LOG_LEVEL_DBG
#endif

/**
 * External-storage persist threshold (compile-time). The RAM ring keeps EVERY
 * entry that passed the runtime filter; only entries with level <= this are
 * copied on to external storage when the ring is flushed. level IS encoded into
 * the entry header (see below), so this filter is applied in the flush path by
 * reading the level back out of each entry -- RAM keeps all, ext keeps a subset.
 *   = N_WW_LOG_LEVEL_WRN : persist only ERR/WRN to ext (default)
 *   = N_WW_LOG_LEVEL_DBG : persist everything to ext
 */
#ifndef N_WW_LOG_EXT_LEVEL_THRESHOLD
#define N_WW_LOG_EXT_LEVEL_THRESHOLD    N_WW_LOG_LEVEL_WRN
#endif

/* Number of runtime-maskable modules (g_ww_log_module_mask is a U32). */
#define N_WW_LOG_MODULE_MAX    32

/* ========= Per-file injected macros (defaults if not injected by build) =========
 * The Makefile injects CURRENT_FILE_ID / CURRENT_MODULE_ID /
 * CURRENT_MODULE_STATIC_EN per source file from ww_log_map.json. */
#ifndef CURRENT_FILE_ID
#define CURRENT_FILE_ID        0
#endif

#ifndef CURRENT_MODULE_ID
#define CURRENT_MODULE_ID      0
#endif

#ifndef CURRENT_MODULE_STATIC_EN
#define CURRENT_MODULE_STATIC_EN    0    /* unregistered file -> logs off */
#endif

/* ========= Encode bit-field layout =========
 *   31             20 19         6 5   4 3      0
 *   +-----------------+------------+-----+--------+
 *   |   file_id (12)  |  line (14) |lv(2)|pcnt(4) |
 *   +-----------------+------------+-----+--------+
 *                       file_id = [ module_id : 5 ][ offset : 7 ]
 *
 * level (2 bits) is encoded so the flush path can decide, per entry, whether to
 * copy it to external storage (see N_WW_LOG_EXT_LEVEL_THRESHOLD) without a side
 * table. That cost 2 bits from param_count: max params is now 15 (was 16).
 *
 * The pack macro and the accessors are mode-independent: the storage layer
 * parses entries (param count, level, file_id) to walk the ring / filter / etc.
 * even in STRING builds. */
#define N_WW_LOG_ENCODE(file_id, line, level, pcnt) \
    ( (((U32)(file_id)  & 0xFFF)  << 20) | \
      (((U32)(line)     & 0x3FFF) << 6)  | \
      (((U32)(level)    & 0x3)    << 4)  | \
      (((U32)(pcnt)     & 0xF)) )

#define N_WW_LOG_FILEID_OF(encoded)    (((encoded) >> 20) & 0xFFF)
#define N_WW_LOG_LINE_OF(encoded)      (((encoded) >> 6)  & 0x3FFF)
#define N_WW_LOG_LEVEL_OF(encoded)     (((encoded) >> 4)  & 0x3)
#define N_WW_LOG_PCNT_OF(encoded)      ((encoded) & 0xF)
#define N_WW_LOG_MODULE_OF(file_id)    (((file_id) >> 7) & 0x1F)
#define N_WW_LOG_OFFSET_OF(file_id)    ((file_id) & 0x7F)

#define N_WW_LOG_ENCODE_MAX_PARAMS     15

/* ========= Control records =========
 * Records the log module writes into its OWN stream, as opposed to entries a
 * call site produced. They are ordinary entries as far as every walker is
 * concerned (the pcnt field gives their length, so the cold-boot scan, the
 * flush packer and the host decoder all step over them unchanged) -- they just
 * use a file_id/line pair that no real call site can occupy:
 *
 *   file_id 0xFFF is never assigned to a source file (gen_log_map.py reserves
 *   it), and lines 0x3FF0..0x3FFF inside it are the control namespace:
 *
 *     0x3FFF  flush marker  [hdr][tick]                        (8 bytes)
 *     0x3FFE  boot record   [hdr][map_id][version][git_id]    (16 bytes)
 *     0x3FF0..0x3FFD  free for future control records
 *
 * The boot record is what makes an archive that spans firmware updates
 * decodable: it stamps each boot with the identity of the map that can decode
 * the entries following it, so the host can switch maps at the right byte
 * offset instead of decoding old entries with a new map (which silently
 * produces plausible but wrong lines). It also marks reboot boundaries. */
#define N_WW_LOG_CTRL_FILE_ID          0xFFF
#define N_WW_LOG_CTRL_LINE_FLUSH       0x3FFF
#define N_WW_LOG_CTRL_LINE_BOOT        0x3FFE

/* [hdr][map_id][BUILD_VERSION][BUILD_GIT_ID]; level ERR so it always passes the
 * ext-persist threshold and reaches external storage whatever it is set to. */
#define N_WW_LOG_BOOT_RECORD_PCNT      3
#define N_WW_LOG_BOOT_RECORD_HDR \
    N_WW_LOG_ENCODE(N_WW_LOG_CTRL_FILE_ID, N_WW_LOG_CTRL_LINE_BOOT, \
                    N_WW_LOG_LEVEL_ERR, N_WW_LOG_BOOT_RECORD_PCNT)
#define N_WW_LOG_BOOT_RECORD_SIZE      (4 + N_WW_LOG_BOOT_RECORD_PCNT * 4)

/*************************** macro definition end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_DEF_H__ */
