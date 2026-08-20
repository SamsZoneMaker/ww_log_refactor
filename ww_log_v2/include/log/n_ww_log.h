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

/* Single public entry point for the v2 log module. Including this header (e.g.
 * from ww_std.h) brings in the whole layered set:
 *   def   - levels / thresholds / encode bit-field / CURRENT_* (no deps)
 *   api   - runtime control (init, level threshold, module mask)
 *   output- output backend function declarations
 *   macro - N_LOG_* call macros + N_*_IF_TRUE helpers (no-ops in DISABLED mode)
 * The layering is acyclic: def <- output <- macro, def <- api. */
#include "n_ww_log_def.h"
#include "n_ww_log_api.h"
#include "n_ww_log_output.h"
#include "n_ww_log_macro.h"

/*************************** type definition start ***************************/
/*************************** type definition end *****************************/


/*************************** declaration start ***************************/
/*************************** declaration end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_H__ */
