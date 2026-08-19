/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* ww_log runtime control API: init plus the dynamic switches (level threshold
 * and per-module mask). Implemented in n_ww_log_control.c. Depends only on
 * n_ww_log_def.h. */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_API_H__
#define __N_WW_LOG_API_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "n_ww_log_def.h"

/*************************** declaration start ***************************/

/* Runtime switch state (defined in n_ww_log_control.c). */
extern U32 g_ww_log_module_mask;       /* one bit per module (0..31) */
extern U8  g_ww_log_level_threshold;   /* drop entries with level > this */

void n_ww_log_init(void);

#ifdef CONFIG_N_LOG_MODE_ENCODE
/* Stamp a boot record (map identity + firmware version) into the stream.
 * Called by n_ww_log_init(), and again whenever the external archive is wiped:
 * an archive with no boot record in front of its entries cannot tell the host
 * which map decodes them. */
void n_ww_log_write_boot_record(void);
U16  n_ww_log_fill_boot_record(U8 *dst);
#endif

void n_ww_log_set_level_threshold(U8 level);
U8   n_ww_log_get_level_threshold(void);

void n_ww_log_set_module_mask(U32 mask);
U32  n_ww_log_get_module_mask(void);

void    n_ww_log_enable_module(U8 module_id);
void    n_ww_log_disable_module(U8 module_id);
WW_BOOL n_ww_log_is_module_enabled(U8 module_id);

/*************************** declaration end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_API_H__ */
