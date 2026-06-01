/**
 * @file drv_in.h
 * @brief DRIVERS module internal header
 * @date 2025-12-17
 *
 * This header provides DRIVERS module-specific function declarations.
 * File IDs are now automatically managed via log_config.json and Makefile.
 */

#ifndef DRV_IN_H
#define DRV_IN_H

#include "ww_log.h"

/* ========== UART Driver API ========== */
void drv_uart_init(void);
void drv_uart_send(void);

/* ========== SPI Driver API ========== */
void drv_spi_init(void);
void drv_spi_transfer(void);

/* ========== I2C Driver API ========== */
void drv_i2c_init(void);
void drv_i2c_read(void);
void drv_i2c_write(void);

#endif /* DRV_IN_H */
