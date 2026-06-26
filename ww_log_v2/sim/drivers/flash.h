/**
 * @file drivers/flash.h
 * @brief Sim stub: SPI-NOR flash driver API.
 *
 * The real driver operates on a "struct device *" (Zephyr-style or custom).
 * In sim mode CONFIG_N_LOG_BACKEND_EXT_MEM is off, so these are never called.
 * The stubs exist only to satisfy includes in n_ww_log_storage.c.
 */

#ifndef __DRIVERS_FLASH_H__
#define __DRIVERS_FLASH_H__

#include "def.h"

struct device; /* opaque in sim */

static inline int flash_read(const struct device *dev, U32 offset, U8 *buf, U32 len)
{
    (void)dev; (void)offset; (void)buf; (void)len;
    return WW_ERR;
}

static inline int flash_write(const struct device *dev, U32 offset, const U8 *buf, U32 len)
{
    (void)dev; (void)offset; (void)buf; (void)len;
    return WW_ERR;
}

static inline int flash_erase(const struct device *dev, U32 offset, U32 size)
{
    (void)dev; (void)offset; (void)size;
    return WW_ERR;
}

#endif /* __DRIVERS_FLASH_H__ */
