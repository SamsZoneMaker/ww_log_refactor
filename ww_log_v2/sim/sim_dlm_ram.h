/**
 * @file sim_dlm_ram.h
 * @brief Declares the static array that backs DLM_MAINTAIN_LOG_BASE_ADDR in sim.
 */

#ifndef __SIM_DLM_RAM_H__
#define __SIM_DLM_RAM_H__

#include "def.h"
#include "dlm_layout.h"

/* Defined in sim_dlm_ram.c */
extern U8 g_sim_dlm_log_ram[DLM_MAINTAIN_LOG_SIZE];

void sim_dlm_ram_clear(void);

#endif /* __SIM_DLM_RAM_H__ */
