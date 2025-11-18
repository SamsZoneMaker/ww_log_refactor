#include "log_filter.h"

uint32_t log_level_mask = (1 << LOG_LEVEL_MAX) - 1;  // all enabled
uint32_t log_module_mask = (1 << MODULE_MAX) - 1;  // all enabled

int log_should_filter(log_level_t level, log_module_t module) {
    return !((log_level_mask & (1 << level)) && (log_module_mask & (1 << module)));
}