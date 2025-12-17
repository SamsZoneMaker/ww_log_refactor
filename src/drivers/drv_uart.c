/**
 * @file drv_uart.c
 * @brief UART driver module
 * @date 2025-11-29
 */

#define CURRENT_FILE_OFFSET  DRV_FILE_UART
#include "drv_in.h"

void drv_uart_init(void)
{
    LOG_INF(CURRENT_LOG_PARAM, "Initializing UART driver...");

    U32 baud_rate = 115200;
#ifdef WW_LOG_MODE_ENCODE
    (void)baud_rate;  /* Suppress unused warning in encode mode */
#endif

    LOG_DBG(CURRENT_LOG_PARAM, "Setting baud rate to %u", baud_rate);
    LOG_INF(CURRENT_LOG_PARAM, "UART driver initialized");
}

void drv_uart_send(int length)
{
#ifdef WW_LOG_MODE_ENCODE
    (void)length;  /* Suppress unused warning in encode mode */
#endif
    LOG_DBG(CURRENT_LOG_PARAM, "Sending data via UART, length=%d bytes", length);
    LOG_INF(CURRENT_LOG_PARAM, "UART transmission complete");
}
