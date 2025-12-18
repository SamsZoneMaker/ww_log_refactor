/**
 * @file drv_i2c.c
 * @brief I2C driver - LOG test only
 * @date 2025-11-29
 */

#include "drv_in.h"

void drv_i2c_init(void)
{
    LOG_INF("I2C driver init");
    LOG_DBG("I2C clock: 400kHz");
}

void drv_i2c_read(void)
{
    LOG_DBG("I2C read in progress");
    LOG_INF("I2C read complete");
}

void drv_i2c_write(void)
{
    LOG_DBG("I2C write in progress");
    LOG_INF("I2C write complete");
}
