/**
 * @file sim_dlm_ram.c
 * @brief Provides the simulated DLM (Data Local Memory) RAM region.
 *
 * On the real RISC-V target the log RAM lives in a noinit/battery-backed
 * memory region at a fixed hardware address.  In the sim we allocate a
 * plain static array and point DLM_MAINTAIN_LOG_BASE_ADDR at it.
 */

#include "sim_dlm_ram.h"
#include <string.h>

U8 g_sim_dlm_log_ram[DLM_MAINTAIN_LOG_SIZE];

void sim_dlm_ram_clear(void)
{
    memset(g_sim_dlm_log_ram, 0, sizeof(g_sim_dlm_log_ram));
}
