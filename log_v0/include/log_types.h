#ifndef LOG_TYPES_H
#define LOG_TYPES_H

#include <stdint.h>

// 类型定义
typedef uint32_t log_code_t;

// enum for modules
typedef enum {
    MODULE_DRIVER = 0,
    MODULE_APP,
    MODULE_UTILS,
    MODULE_MAX
} log_module_t;

// level
typedef enum {
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_MAX
} log_level_t;

// dir id for file id split
#define DIR_ID_BITS 4
#define FILE_ID_IN_DIR_BITS (FILE_ID_BITS - DIR_ID_BITS)

typedef enum {
    DIR_INCLUDE = 0,
    DIR_SRC,
    DIR_MODULES,
    DIR_TEST,
    DIR_TOOLS,
    DIR_DOCS,
    DIR_BUILD,
    DIR_MAX
} log_dir_t;

#endif