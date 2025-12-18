/**
 * @file drv_spi.c
 * @brief SPI driver - LOG test only
 * @date 2025-11-29
 */

#include "drv_in.h"

void drv_spi_init(void)
{
    LOG_INF("SPI driver init");
    LOG_DBG("SPI clock: 1MHz");
}

void drv_spi_transfer(void)
{
    LOG_DBG("SPI transfer in progress");
    LOG_INF("SPI transfer complete");
}
