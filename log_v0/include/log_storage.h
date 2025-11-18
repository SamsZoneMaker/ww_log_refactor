#ifndef LOG_STORAGE_H
#define LOG_STORAGE_H

#include <stdint.h>

#define LOG_BUFFER_SIZE 1024

extern uint8_t log_buffer[LOG_BUFFER_SIZE];
extern uint32_t log_buffer_index;

void log_store_to_ram(uint32_t data);
void log_flush_to_external();  // 模拟

#endif