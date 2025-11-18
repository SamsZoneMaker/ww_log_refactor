#include "log_storage.h"
#include <stdio.h>

uint8_t log_buffer[LOG_BUFFER_SIZE];
uint32_t log_buffer_index = 0;

void log_store_to_ram(uint32_t data) {
    if (log_buffer_index < LOG_BUFFER_SIZE - 4) {
        *(uint32_t*)&log_buffer[log_buffer_index] = data;
        log_buffer_index += 4;
    }
}

void log_flush_to_external() {
    // 模拟写入EEPROM
    printf("Flushing %d bytes to external storage\n", log_buffer_index);
    log_buffer_index = 0;
}