#ifndef LOG_API_H
#define LOG_API_H

#include "log_config.h"
#include "log_types.h"

// 宏定义日志函数
#if LOG_MODE == LOG_MODE_STR
#define LOG_INFO(module, fmt, ...) _LOG_STR(LOG_LEVEL_INFO, module, fmt, ##__VA_ARGS__)
#define LOG_WARN(module, fmt, ...) _LOG_STR(LOG_LEVEL_WARN, module, fmt, ##__VA_ARGS__)
#define LOG_ERROR(module, fmt, ...) _LOG_STR(LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(module, ...) _LOG_ECODE(LOG_LEVEL_INFO, module, ##__VA_ARGS__)
#define LOG_WARN(module, ...) _LOG_ECODE(LOG_LEVEL_WARN, module, ##__VA_ARGS__)
#define LOG_ERROR(module, ...) _LOG_ECODE(LOG_LEVEL_ERROR, module, ##__VA_ARGS__)
#endif

#define _LOG_STR(level, module, fmt, ...) log_str(level, module, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define _LOG_ECODE(level, module, ...) log_encode(level, module, __FILE__, __LINE__, ##__VA_ARGS__)

// 函数声明
void log_str(log_level_t level, log_module_t module, const char* file, int line, const char* fmt, ...);
void log_encode(log_level_t level, log_module_t module, const char* file, int line, const char* fmt, ...);

#endif