

/*************************** description start ***************************/
/* to add description for this file if needed */
/*************************** description end *****************************/

// #include <>
// #include ""
#include "ww_std.h"
#include "log/n_ww_log.h"
#include "log/n_ww_log_control.h"
#include "log/n_ww_log_storage.h"

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
    log_ram_init(WW_TRUE); /* try to preserve existing data (hot restart) */
#endif
#if (CONFIG_N_LOG_BACKEND_EXT_MEM == 1)
    log_flush_task_init();
#endif
}

/*************************** global function end *****************************/
