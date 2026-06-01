/**
 * @file drv_uart.c
 * @brief UART driver - LOG test only
 * @date 2025-11-29
 */

#include "drv_in.h"

void drv_uart_init(void)
{
    LOG_INF("UART driver init");
    LOG_DBG("UART baud rate: 115200");
}

void drv_uart_send(void)
{
    LOG_DBG("UART sending data");
    LOG_INF("UART TX complete");
}
