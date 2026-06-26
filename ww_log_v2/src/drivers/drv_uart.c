/**
 * @file drv_uart.c
 * @brief Simulated UART driver (DRIVERS module, registered in log_config.json).
 */

#include "log/n_ww_log.h"
#include "drv_in.h"

void drv_uart_init(void)
{
    N_LOG_INF("UART driver init");
    N_LOG_DBG("UART baud=115200");
}

void drv_uart_send(int length)
{
    if (length > 256)
    {
        N_LOG_WRN("UART send: length=%d exceeds limit", length);
    }
    N_LOG_INF("UART send: length=%d", length);
}
