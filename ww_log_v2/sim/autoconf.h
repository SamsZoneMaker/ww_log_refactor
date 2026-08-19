/**
 * @file autoconf.h
 * @brief Sim replacement for the Kconfig-generated autoconf.h.
 *
 * Force-included on every translation unit by the Makefile (-include autoconf.h),
 * the same way the on-target Kconfig build injects its CONFIG_* symbols.
 *
 * There is nothing to edit here any more: the log module's mode, backends and
 * tuning all come from the `build` block of scripts/log/log_config.json, which
 * gen_log_map.py turns into output/log_autoconf.h. One data file is the source
 * of truth, and the mode is a word ("encode"/"string"/"disabled") that the
 * generator validates -- the old three-mutually-exclusive-#defines arrangement
 * silently fell through to DISABLED if you forgot to uncomment one.
 *
 * On target the same generator output can be dropped in beside the Kconfig
 * autoconf.h, or the `build` block can be rendered as a defconfig fragment.
 */

#ifndef __AUTOCONF_H__
#define __AUTOCONF_H__

#include "log_autoconf.h"   /* generated -- edit scripts/log/log_config.json */

#endif /* __AUTOCONF_H__ */
