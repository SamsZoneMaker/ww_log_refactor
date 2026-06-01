/**
 * @file demo_in.h
 * @brief DEMO module internal header
 * @date 2025-12-17
 *
 * This header provides DEMO module-specific function declarations.
 * File IDs are now automatically managed via log_config.json and Makefile.
 */

#ifndef DEMO_IN_H
#define DEMO_IN_H

#include "ww_log.h"

/* ========== Module API ========== */

void demo_init(void);
void demo_process(int task_id);

#endif /* DEMO_IN_H */
