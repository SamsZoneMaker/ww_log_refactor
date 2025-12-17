/**
 * @file drv_i2c.c
 * @brief I2C driver module
 * @date 2025-11-29
 */

#define CURRENT_FILE_OFFSET  DRV_FILE_I2C
#include "drv_in.h"

void drv_i2c_init(void)
{
    LOG_INF(CURRENT_LOG_PARAM, "Initializing I2C driver...");

    U32 clock_speed = 400000;  /* 400 kHz */
#ifdef WW_LOG_MODE_ENCODE
    (void)clock_speed;  /* Suppress unused warning in encode mode */
#endif

    LOG_DBG(CURRENT_LOG_PARAM, "Setting I2C clock to %u Hz", clock_speed);
    LOG_INF(CURRENT_LOG_PARAM, "I2C driver initialized");
}

void drv_i2c_read(int device_addr, int reg_addr)
{
#ifdef WW_LOG_MODE_ENCODE
    (void)device_addr;  /* Suppress unused warning in encode mode */
    (void)reg_addr;     /* Suppress unused warning in encode mode */
#endif
    LOG_DBG(CURRENT_LOG_PARAM, "I2C read from device 0x%02X, reg 0x%02X", device_addr, reg_addr);
    LOG_INF(CURRENT_LOG_PARAM, "I2C read complete");
}

void drv_i2c_write(int device_addr, int data)
{
#ifdef WW_LOG_MODE_ENCODE
    (void)device_addr;  /* Suppress unused warning in encode mode */
    (void)data;         /* Suppress unused warning in encode mode */
#endif
    LOG_DBG(CURRENT_LOG_PARAM, "I2C write to device 0x%02X, data=0x%02X", device_addr, data);
    LOG_INF(CURRENT_LOG_PARAM, "I2C write complete");
}
