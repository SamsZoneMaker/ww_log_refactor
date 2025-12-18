/**
 * @file brom_loader.c
 * @brief BROM firmware loader
 * @date 2025-11-29
 */

#include "brom_in.h"

void brom_load_firmware(void)
{
    /* File ID is automatically injected by Makefile via -DCURRENT_FILE_ID=xxx */
    LOG_INF("Loading firmware image...");
    int image_size = 65536;

    LOG_DBG("Reading image header...");

    if (image_size == 0) {
        LOG_ERR("Invalid firmware image size!");
        return;
    }

    LOG_INF("Image loaded, size=%d bytes", image_size);
}

void brom_verify_image(void)
{
    LOG_DBG("Verifying firmware image...");

    unsigned int checksum = 0x12345678;
    unsigned int expected = 0x12345678;

    if (checksum != expected) {
        LOG_ERR("Checksum mismatch, got=0x%X, expected=0x%X", checksum, expected);
        return;
    }

    LOG_INF("Image verification passed");
}

void brom_jump_to_app(void)
{
    LOG_INF("Jumping to application...");

    unsigned int app_address = 0x08000000;
#ifdef ENABLE_MEMORY_PROTECTION
    LOG_DBG("Disabling memory protection...");
#endif
    LOG_DBG("Application entry point: 0x%08X", app_address);

    LOG_INF("Boot loader complete");
}
