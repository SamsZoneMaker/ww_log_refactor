/**
 * @file ww_log_ctrl.h
 * @brief Runtime control: dynamic module mask and level threshold.
 *
 * Two independent switches are checked inside the output functions (centralised,
 * keeps call sites tiny). The switch key is the module_id (stable, from
 * log_config.json), never the file offset, so file_id locking/drift does not
 * affect the switches. See CLAUDE.md §4.
 */

#ifndef WW_LOG_CTRL_H
#define WW_LOG_CTRL_H

#include "type.h"
#include "auto_file_ids.h"  /* generated: WW_LOG_MODULE_* ids + WW_LOG_MODULE_MAX */

/* ========== Dynamic Module Mask ========== */

/** Each bit enables one module (0-31). Default: all enabled. */
extern U32 g_ww_log_module_mask;

void ww_log_set_module_mask(U32 mask);
U32  ww_log_get_module_mask(void);
void ww_log_enable_module(U8 module_id);
void ww_log_disable_module(U8 module_id);
U8   ww_log_is_module_enabled(U8 module_id);

/* ========== Level Threshold ========== */

/** Logs with level > threshold are filtered. Default: DBG (allow all). */
extern U8 g_ww_log_level_threshold;

void ww_log_set_level_threshold(U8 level);
U8   ww_log_get_level_threshold(void);

#endif /* WW_LOG_CTRL_H */
