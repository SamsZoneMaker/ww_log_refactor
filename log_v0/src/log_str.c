#include "log_api.h"
#include "log_filter.h"
#include <stdio.h>
#include <stdarg.h>

// level string
static const char* level_str[LOG_LEVEL_MAX] = {"INFO", "WARN", "ERROR"};

void log_str(log_level_t level, log_module_t module, const char* file, int line, const char* fmt, ...) {
    if (log_should_filter(level, module)) return;
    printf("[%s] %s:%d: ", level_str[level], file, line);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}