/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* Compatibility shim. The control header was split into the layered set
 *   n_ww_log_def.h    - levels / thresholds / encode bit-field / CURRENT_*
 *   n_ww_log_api.h    - runtime control API (init, switches)
 *   n_ww_log_macro.h  - N_LOG_* call macros + N_*_IF_TRUE helpers
 *   n_ww_log_output.h - output backend function declarations
 * to break the old control<->output include cycle. This header now just pulls
 * those in so existing `#include "log/n_ww_log_control.h"` sites keep working;
 * new code should include the specific layer it needs. */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_CONTROL_H__
#define __N_WW_LOG_CONTROL_H__

#include "n_ww_log_def.h"
#include "n_ww_log_api.h"
#include "n_ww_log_macro.h"

#endif /* __N_WW_LOG_CONTROL_H__ */
