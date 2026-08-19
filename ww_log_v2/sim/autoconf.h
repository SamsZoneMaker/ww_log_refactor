/**
 * @file autoconf.h
 * @brief Sim replacement for the Kconfig-generated autoconf.h.
 *
 * Force-included on every translation unit by the Makefile (-include autoconf.h),
 * the same way the on-target Kconfig build injects its CONFIG_* symbols.
 *
 * There is nothing to edit here: the log module's mode, backends and tuning all
 * come from sim/log.conf, which sim/conf_to_autoconf.py renders into
 * output/log_autoconf.h -- the same step the firmware build performs on its own
 * Kconfig .conf files.
 *
 * log.conf uses Kconfig .conf syntax and the firmware Kconfig's symbol names, so a
 * sim config and a firmware defconfig fragment are interchangeable.
 */

#ifndef __AUTOCONF_H__
#define __AUTOCONF_H__

#include "log_autoconf.h"   /* generated -- edit scripts/log/log_config.json */

#endif /* __AUTOCONF_H__ */
