/**
 * @file drv_uart.c
 * @brief DRIVERS module - UART driver (demo).
 */

#include "drv_uart.h"

void drv_uart_init(void)
{
    LOG_INF("UART init, baud=%d", 115200);
    LOG_DBG("UART FIFO depth=%d", 16);
}

void drv_uart_send(int length)
{
    LOG_DBG("UART sending, length=%d", length);
    if (length > 256) {
        LOG_WRN("UART payload large, length=%d", length);
    }
    LOG_INF("UART sent %d bytes", length);
}
