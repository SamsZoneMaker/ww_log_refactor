/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* to add description for this file if needed */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_H__
#define __N_WW_LOG_H__

#ifdef __cplusplus
extern "C"
{
#endif

//#include <>
//#include ""
#include "autoconf.h"
#include "n_ww_log_control.h"


/*************************** macro definition start ***************************/

#if defined(CONFIG_N_LOG_MODE_STRING) || defined(CONFIG_N_LOG_MODE_ENCODE)
#include "n_ww_log_output.h"
#endif

#if defined(CONFIG_N_LOG_MODE_ENCODE) && defined(CONFIG_N_LOG_BACKEND_RAM)
#include "n_ww_log_storage.h"
#endif

/* In DISABLED mode the N_LOG_* macros expand to no-ops; they are defined in
 * n_ww_log_output.h (pulled in via n_ww_log_control.h) so every TU that uses the
 * logging/return-code macros sees them, with or without including this header. */

/*************************** macro definition end *****************************/


/*************************** type definition start ***************************/
/*************************** type definition end *****************************/


/*************************** declaration start ***************************/
/*************************** declaration end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_H__ */
