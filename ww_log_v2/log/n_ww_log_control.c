

/*************************** description start ***************************/
/* to add description for this file if needed */
/*************************** description end *****************************/

// #include <>
// #include ""
#include "ww_std.h"
#include "log/n_ww_log_api.h"       /* this module implements the control API */
#include "log/n_ww_log_storage.h"   /* log_ram_init (RAM backend) */
#include "log/n_ww_log_task.h"      /* log_lock_init / log_flush_task_init */

#ifdef CONFIG_N_LOG_MODE_ENCODE
#include "log/n_ww_log_output.h"    /* ww_log_backend_emit (boot record) */
#include "log_map_id.h"             /* generated: N_WW_LOG_MAP_ID */
#include "version.h"                /* project:   BUILD_VERSION / BUILD_GIT_ID */

/* Tolerate a project whose version.h lacks one of these rather than failing to
 * build; a zero simply reads as "unknown" on the host side. */
#ifndef BUILD_VERSION
#define BUILD_VERSION    0u
#endif
#ifndef BUILD_GIT_ID
#define BUILD_GIT_ID     0u
#endif
#endif /* CONFIG_N_LOG_MODE_ENCODE */

/*************************** global variable start ***************************/
/* to be used in all files */

/**
 * Global module mask variable
 * Each bit controls whether a module is enabled at runtime
 * Default: 0xFFFFFFFF (all modules enabled)
 */
U32 g_ww_log_module_mask = 0xFFFFFFFF;

U8 g_ww_log_level_threshold = N_WW_LOG_LEVEL_DBG; /* allow all by default */

/*************************** global variable end *****************************/

/*************************** macro definition start ***************************/
/* to be used only in this file */
/*************************** macro definition end *****************************/

/*************************** type definition start ***************************/
/* to be used only in this file */
/*************************** type definition end *****************************/

/*************************** declaration start ***************************/
/* to be used only in this file */
/*************************** declaration end *****************************/

/*************************** static variable start ***************************/
/* to be used only in this file */
/*************************** static variable end *****************************/

/*************************** static function start ***************************/
/* to be used only in this file */
/*************************** static function end *****************************/


/*************************** global function start ***************************/

#ifdef CONFIG_N_LOG_MODE_ENCODE
/**
 * @brief Stamp one boot record into the log stream (see n_ww_log_def.h).
 *
 * Emitted through ww_log_backend_emit() rather than n_ww_log_encode_output()
 * on purpose: that entry point applies no module-mask or level filtering, so
 * the stamp survives whatever the application later does with the runtime
 * switches, and it fans out to every enabled backend at once -- the record
 * shows up in the RAM ring (and so in a RAM dump), on UART, and reaches
 * external storage through the ordinary flush path. No dedicated write path
 * to the device is needed, and no walker needs to learn a new shape: with
 * pcnt=3 the cold-boot scan, the flush packer and the host decoder all step
 * over it as a plain 16-byte entry.
 *
 * Ordering holds on both boot kinds. Cold boot: the ring was cleared, so this
 * is the first entry of the boot. Warm restart: entries not yet flushed still
 * sit in the ring, this record lands after them, and the flush drains oldest
 * first -- so those stragglers stay attributed to the PREVIOUS boot record,
 * which is where they belong.
 */
void n_ww_log_write_boot_record(void)
{
    U32 params[N_WW_LOG_BOOT_RECORD_PCNT];

    params[0] = (U32)N_WW_LOG_MAP_ID;
    params[1] = (U32)BUILD_VERSION;
    params[2] = (U32)BUILD_GIT_ID;

    ww_log_backend_emit(N_WW_LOG_BOOT_RECORD_HDR, params,
                        N_WW_LOG_BOOT_RECORD_PCNT, N_WW_LOG_LEVEL_ERR);
}

/**
 * @brief Serialise a boot record into `dst` (N_WW_LOG_BOOT_RECORD_SIZE bytes).
 * @return bytes written.
 * @note Used by the flush path to stamp an EMPTY archive before its first
 *       entries, which is what makes "entries in the archive are always
 *       preceded by a boot record" an invariant of the writer rather than
 *       something that depends on init ordering (a wipe followed by a RAM ring
 *       clear would otherwise drop the stamp before it was ever flushed).
 */
U16 n_ww_log_fill_boot_record(U8 *dst)
{
    U32 *w = (U32 *)dst;

    w[0] = (U32)N_WW_LOG_BOOT_RECORD_HDR;
    w[1] = (U32)N_WW_LOG_MAP_ID;
    w[2] = (U32)BUILD_VERSION;
    w[3] = (U32)BUILD_GIT_ID;
    return N_WW_LOG_BOOT_RECORD_SIZE;
}
#endif /* CONFIG_N_LOG_MODE_ENCODE */

/**
 * @brief Get current log level threshold
 * @return Current threshold value
 */
U8 n_ww_log_get_level_threshold(void)
{
    return g_ww_log_level_threshold;
}

/**
 * @brief Set global log level threshold
 * @param level New threshold (N_WW_LOG_LEVEL_ERR/WRN/INF/DBG)
 */
void n_ww_log_set_level_threshold(U8 level)
{
    if (level <= N_WW_LOG_LEVEL_DBG)
    {
        g_ww_log_level_threshold = level;
    }
}

/**
 * @brief Get current module mask
 */
U32 n_ww_log_get_module_mask(void)
{
    return g_ww_log_module_mask;
}

/**
 * @brief Set module mask to control which modules are enabled
 * @param mask 32-bit mask, each bit controls one module
 * * Examples:
 * ww_log_set_module_mask(0xFFFFFFFF); // Enable all
 * ww_log_set_module_mask(0x00000000); // Disable all
 * ww_log_set_module_mask(0x0000003F); // Enable modules 0~5
 */
void n_ww_log_set_module_mask(U32 mask)
{
    g_ww_log_module_mask = mask;
    // ww_printf("The module mask now is: %u", g_ww_log_module_mask);
}

/**
 * @brief Enable a specific module
 */
void n_ww_log_enable_module(U8 module_id)
{
    if (module_id < N_WW_LOG_MODULE_MAX)
    {
        g_ww_log_module_mask |= (1U << module_id);
    }
}

/**
 * @brief Disable a specific module
 */
void n_ww_log_disable_module(U8 module_id)
{
    if (module_id < N_WW_LOG_MODULE_MAX)
    {
        g_ww_log_module_mask &= ~(1U << module_id);
    }
}

/**
 * @brief Check if a specific module is enabled
 */
WW_BOOL n_ww_log_is_module_enabled(U8 module_id)
{
    if (module_id >= N_WW_LOG_MODULE_MAX)
    {
        return WW_ERR;
    }
    return (g_ww_log_module_mask & (1U << module_id)) ? WW_TRUE : WW_FALSE;
}

void n_ww_log_init(void)
{
    // ww_printf("LOG: ww_log v1 init (mode: ");
    // #if defined(CONFIG_N_LOG_MODE_ENCODE)
    // ww_printf("ENCODE");
    // #elif defined(CONFIG_N_LOG_MODE_STRING)
    // ww_printf("STRING");
    // #elif defined(CONFIG_N_LOG_MODE_DISABLE)
    // ww_printf("DISABLED");
    // #else
    // ww_printf("UNKNOWN MODE");
    // #endif
    // ww_printf(", backends:");
    // #if (CONFIG_N_LOG_BACKEND_UART == 1)
    // ww_printf(" UART");
    // #endif
    // #if (CONFIG_N_LOG_BACKEND_RAM == 1)
    // ww_printf(" RAM");
    // #endif
    // #if (CONFIG_N_LOG_BACKEND_EXT_MEM == 1)
    // ww_printf(" STORAGE");
    // #endif
    // ww_printf(")\n");

#if (CONFIG_N_LOG_BACKEND_RAM == 1)
    /* Create the mutex up front so the RAM ring is protected even in builds
     * without the external-storage flush task. */
    log_lock_init();
    /* force_clear = WW_FALSE: hot-restart preserve. log_ram_init() validates the
     * existing header (magic + checksum) and keeps prior contents on a warm
     * reboot; only a failed validation (or cold boot) re-initialises. Previously
     * this passed WW_TRUE, which always wiped the buffer and defeated crash-log
     * recovery (see bug #8). */
    log_ram_init(WW_FALSE);
#endif
#if (CONFIG_N_LOG_BACKEND_EXT_MEM == 1)
    log_flush_task_init();
#endif

#ifdef CONFIG_N_LOG_MODE_ENCODE
    /* After the ring exists, before the application logs anything: everything
     * that follows in the stream belongs to this firmware build. */
    n_ww_log_write_boot_record();
#endif
}

/*************************** global function end *****************************/
