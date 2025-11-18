#ifndef LOG_FILTER_H
#define LOG_FILTER_H

#include "log_types.h"

extern uint32_t log_level_mask;
extern uint32_t log_module_mask;

int log_should_filter(log_level_t level, log_module_t module);

#endif