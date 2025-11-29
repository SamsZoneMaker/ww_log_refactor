/**
 * @file drv_uart.c
 * @brief UART driver module
 * @date 2025-11-29
 */

#define CURRENT_FILE_OFFSET  DRV_FILE_UART
#include "drv_in.h"

void drv_uart_init(void)
{
    LOG_INF(CURRENT_MODULE_TAG, "Initializing UART driver...");

    U32 baud_rate = 115200;

    LOG_DBG(CURRENT_MODULE_TAG, "Setting baud rate to %u", baud_rate);
    LOG_INF(CURRENT_MODULE_TAG, "UART driver initialized");
}

void drv_uart_send(int length)
{
    LOG_DBG(CURRENT_MODULE_TAG, "Sending data via UART, length=%d bytes", length);
    LOG_INF(CURRENT_MODULE_TAG, "UART transmission complete");
}
