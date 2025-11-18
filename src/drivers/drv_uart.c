/**
 * @file drv_uart.c
 * @brief UART driver module
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_DRV_UART
#define CURRENT_MODULE_ID WW_LOG_MOD_DRIVERS

/**
 * @brief Initialize UART driver
 */
void drv_uart_init(void)
{
    TEST_LOG_INF_MSG("Initializing UART driver...");

    int baud_rate = 115200;

    TEST_LOG_DBG_MSG("Configuring UART...");

    if (baud_rate < 9600) {
        TEST_LOG_WRN_MSG("UART baud rate too low, rate=%d", baud_rate);
    }

    TEST_LOG_INF_MSG("UART initialized, baud=%d", baud_rate);
}

/**
 * @brief Send data via UART
 */
void drv_uart_send(int length)
{
    if (length <= 0) {
        TEST_LOG_ERR_MSG("Invalid UART send length!");
        return;
    }

    TEST_LOG_DBG_MSG("Sending UART data...");

    int sent_bytes = length;

    TEST_LOG_INF_MSG("UART data sent, bytes=%d", sent_bytes);
}
