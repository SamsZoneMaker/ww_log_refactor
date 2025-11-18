#include "../../include/log_api.h"

void uart_init() {
    LOG_INFO(MODULE_DRIVER, "UART init with baud %d", 9600);
}