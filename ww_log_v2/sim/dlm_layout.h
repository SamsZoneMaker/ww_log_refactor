/**
 * @file dlm_layout.h
 * @brief Sim replacement for the fw DLM (Data Local Memory) layout header.
 *
 * On the real RISC-V target, DLM_MAINTAIN_LOG_BASE_ADDR is a fixed hardware
 * address in a battery-backed / noinit SRAM region.  In the sim we redirect
 * it to a plain static array so log_ram_init/write can operate normally.
 */

#ifndef __DLM_LAYOUT_H__
#define __DLM_LAYOUT_H__

#include "def.h"
#include <stdint.h>

/* ======================================================================
 * Size of the log RAM region.
 * Must be large enough for LOG_RAM_HEADER_T (32 bytes) + data area.
 * TODO: VERIFY this matches the actual DLM region size on target.
 * ====================================================================== */
#define DLM_MAINTAIN_LOG_SIZE   (4096U)

/* ======================================================================
 * The sim "DLM" region: a plain static array defined in sim_dlm_ram.c.
 * DLM_MAINTAIN_LOG_BASE_ADDR is cast to the same type the fw code expects
 * (a raw address integer that is then cast to a struct pointer).
 * ====================================================================== */
extern U8 g_sim_dlm_log_ram[DLM_MAINTAIN_LOG_SIZE];

#define DLM_MAINTAIN_LOG_BASE_ADDR  ((uintptr_t)(g_sim_dlm_log_ram))

#endif /* __DLM_LAYOUT_H__ */
