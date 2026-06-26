/**
 * @file drivers/flash.h
 * @brief Sim SPI-NOR flash driver API (file-backed, implemented in
 *        sim_ext_storage.c). Signatures match the firmware driver:
 *        (dev, offset, buf, len). Only used when the EXT_MEM backend is on.
 */

#ifndef __DRIVERS_FLASH_H__
#define __DRIVERS_FLASH_H__

#include "def.h"

struct device; /* opaque handle, concrete in sim_ext_storage.c */

int flash_read(const struct device *dev, U32 offset, U8 *buf, U32 len);
int flash_write(const struct device *dev, U32 offset, U8 *buf, U32 len);
int flash_erase(const struct device *dev, U32 offset, U32 size);

#endif /* __DRIVERS_FLASH_H__ */
