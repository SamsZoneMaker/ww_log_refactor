/**
 * @file drv_i2c.c
 * @brief I2C driver module
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_DRV_I2C
#define CURRENT_MODULE_ID WW_LOG_MOD_DRIVERS

/**
 * @brief Initialize I2C driver
 */
void drv_i2c_init(void)
{
    TEST_LOG_INF_MSG("Initializing I2C driver...");

    int clock_speed = 400000;  /* 400 kHz */

    TEST_LOG_DBG_MSG("Configuring I2C...");

    if (clock_speed > 1000000) {
        TEST_LOG_WRN_MSG("I2C clock speed too high, speed=%d", clock_speed);
    }

    TEST_LOG_INF_MSG("I2C initialized, speed=%d", clock_speed);
}

/**
 * @brief Read from I2C device
 */
void drv_i2c_read(int device_addr, int reg_addr)
{
    TEST_LOG_DBG_MSG("Reading from I2C device...");

    if (device_addr < 0 || device_addr > 127) {
        TEST_LOG_ERR_MSG("Invalid I2C device address!");
        return;
    }

    TEST_LOG_INF_MSG("I2C read completed, addr=0x%02X, reg=0x%02X", device_addr, reg_addr);
}

/**
 * @brief Write to I2C device
 */
void drv_i2c_write(int device_addr, int data)
{
    if (device_addr < 0 || device_addr > 127) {
        TEST_LOG_ERR_MSG("Invalid I2C device address!");
        return;
    }

    TEST_LOG_DBG_MSG("Writing to I2C device...");

    TEST_LOG_INF_MSG("I2C write completed, addr=0x%02X, data=%d", device_addr, data);
}
