/**
 * @file brom_loader.c
 * @brief Boot ROM loader module
 * @date 2025-11-29
 */

/* Use default offset for this file */
#include "brom_in.h"

void brom_loader_load(void)
{
    LOG_INF(CURRENT_LOG_PARAM, "Loading firmware image...");
    int image_size = 65536;

    LOG_DBG(CURRENT_LOG_PARAM, "Reading image header...");

    if (image_size == 0) {
        LOG_ERR(CURRENT_LOG_PARAM, "Invalid firmware image size!");
        return;
    }

    LOG_INF(CURRENT_LOG_PARAM, "Image loaded, size=%d bytes", image_size);
}

void brom_loader_verify(void)
{
    LOG_DBG(CURRENT_LOG_PARAM, "Verifying firmware image...");

    int checksum = 0xABCD;
    int expected = 0xABCD;

    if (checksum != expected) {
        LOG_ERR(CURRENT_LOG_PARAM, "Checksum mismatch, got=0x%X, expected=0x%X", checksum, expected);
        return;
    }

    LOG_INF(CURRENT_LOG_PARAM, "Image verification passed");
}

void brom_loader_jump(void)
{
    LOG_INF(CURRENT_LOG_PARAM, "Jumping to application...");

    U32 app_address = 0x08000000;
#ifdef WW_LOG_MODE_ENCODE
    (void)app_address;  /* Suppress unused warning in encode mode */
#endif
    LOG_DBG(CURRENT_LOG_PARAM, "Application entry point: 0x%08X", app_address);

    LOG_INF(CURRENT_LOG_PARAM, "Boot loader complete");
}
