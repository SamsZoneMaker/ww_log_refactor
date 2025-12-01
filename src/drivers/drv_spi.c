/**
 * @file drv_spi.c
 * @brief SPI driver module
 * @date 2025-11-29
 */

#define CURRENT_FILE_OFFSET  DRV_FILE_SPI
#include "drv_in.h"

void drv_spi_init(void)
{
    LOG_INF(CURRENT_LOG_PARAM, "Initializing SPI driver...");

    U32 clock_speed = 1000000;  /* 1 MHz */
#ifdef WW_LOG_MODE_ENCODE
    (void)clock_speed;  /* Suppress unused warning in encode mode */
#endif

    LOG_DBG(CURRENT_LOG_PARAM, "Setting SPI clock to %u Hz", clock_speed);
    LOG_INF(CURRENT_LOG_PARAM, "SPI driver initialized");
}

void drv_spi_transfer(int tx_len, int rx_len)
{
#ifdef WW_LOG_MODE_ENCODE
    (void)tx_len;   /* Suppress unused warning in encode mode */
    (void)rx_len;   /* Suppress unused warning in encode mode */
#endif
    LOG_DBG(CURRENT_LOG_PARAM, "SPI transfer: tx=%d bytes, rx=%d bytes", tx_len, rx_len);
    LOG_INF(CURRENT_LOG_PARAM, "SPI transfer complete");
}
