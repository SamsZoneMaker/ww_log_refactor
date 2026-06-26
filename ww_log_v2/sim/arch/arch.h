/**
 * @file arch/arch.h
 * @brief Sim stub: architecture-specific helpers for RISC-V / Andes N25.
 *
 * On target, ww_cycle_get_32() reads the RISC-V cycle CSR.
 * In the sim we substitute a monotonically increasing counter.
 */

#ifndef __ARCH_ARCH_H__
#define __ARCH_ARCH_H__

#include "def.h"
#include <time.h>

static inline U32 ww_cycle_get_32(void)
{
    return (U32)clock();
}

#endif /* __ARCH_ARCH_H__ */
