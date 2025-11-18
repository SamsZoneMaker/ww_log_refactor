/**
 * @file drv_spi.c
 * @brief SPI driver module
 */

#include "ww_log.h"

#define CURRENT_FILE_ID   FILE_ID_DRV_SPI
#define CURRENT_MODULE_ID WW_LOG_MOD_DRIVERS

/**
 * @brief Initialize SPI driver
 */
void drv_spi_init(void)
{
    TEST_LOG_INF_MSG("Initializing SPI driver...");

    int clock_speed = 1000000;  /* 1 MHz */
    int mode = 0;

    TEST_LOG_DBG_MSG("Configuring SPI...");

    if (mode > 3) {
        TEST_LOG_ERR_MSG("Invalid SPI mode!");
        return;
    }

    TEST_LOG_INF_MSG("SPI initialized, speed=%d, mode=%d", clock_speed, mode);
}

/**
 * @brief Transfer data via SPI
 */
void drv_spi_transfer(int tx_len, int rx_len)
{
    TEST_LOG_DBG_MSG("Starting SPI transfer...");

    if (tx_len != rx_len) {
        TEST_LOG_WRN_MSG("SPI transfer length mismatch, tx=%d, rx=%d", tx_len, rx_len);
    }

    TEST_LOG_INF_MSG("SPI transfer completed");
}
