/**
 * @file drv_uart.h
 * @brief DRIVERS module - UART driver (demo).
 */

#ifndef DRV_UART_H
#define DRV_UART_H

#include "ww_log.h"

void drv_uart_init(void);
void drv_uart_send(int length);

#endif /* DRV_UART_H */
