#include "log_api.h"
#include "file_ids.h"
#include "log_filter.h"
#include "log_storage.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

uint32_t get_file_id(const char* file) {
    // Extract relative path from __FILE__
    const char* rel = strstr(file, "workspace/log/");
    if (rel) {
        rel += strlen("workspace/log/");
    } else {
        rel = file;
    }
    for (int i = 0; i < FILE_ID_MAX && file_names[i] != NULL; i++) {
        if (strcmp(file_names[i], rel) == 0) return i;
    }
    return 0;
}

void log_encode(log_level_t level, log_module_t module, const char* file, int line, const char* fmt, ...) {
    if (log_should_filter(level, module)) return;
    uint32_t file_id = get_file_id(file);
    va_list args;
    va_start(args, fmt);
    // 跳过fmt，获取data
    uint32_t data = va_arg(args, uint32_t);  // 假设一个data
    va_end(args);
    uint32_t code = (file_id << FILE_ID_SHIFT) | (line << LINE_SHIFT) | 1;  // data len=1
    // 输出到UART，模拟
    printf("0x%08X 0x%08X\n", code, data);
    // 存储到RAM
    log_store_to_ram(code);
    log_store_to_ram(data);
}