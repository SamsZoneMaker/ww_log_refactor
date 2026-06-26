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

#if !defined(CONFIG_N_LOG_MODE_STRING) && !defined(CONFIG_N_LOG_MODE_ENCODE)
#define N_LOG_ERR(...)    do { } while(0)
#define N_LOG_WRN(...)    do { } while(0)
#define N_LOG_INF(...)    do { } while(0)
#define N_LOG_DBG(...)    do { } while(0)
#endif

/*************************** macro definition end *****************************/


/*************************** type definition start ***************************/
/*************************** type definition end *****************************/


/*************************** declaration start ***************************/
/*************************** declaration end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_H__ */
