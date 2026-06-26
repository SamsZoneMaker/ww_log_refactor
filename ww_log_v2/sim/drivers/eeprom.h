/**
 * @file drivers/eeprom.h
 * @brief Sim stub: I2C EEPROM driver API.
 *
 * Never called in sim (CONFIG_N_LOG_BACKEND_EXT_MEM is off).
 * Stubs exist only to satisfy the unconditional includes in n_ww_log_storage.c.
 */

#ifndef __DRIVERS_EEPROM_H__
#define __DRIVERS_EEPROM_H__

#include "def.h"

struct device; /* opaque in sim, matches flash.h declaration */

static inline int eeprom_read(const struct device *dev, U32 offset, U8 *buf, U32 len)
{
    (void)dev; (void)offset; (void)buf; (void)len;
    return WW_ERR;
}

static inline int eeprom_write(const struct device *dev, U32 offset, const U8 *buf, U32 len)
{
    (void)dev; (void)offset; (void)buf; (void)len;
    return WW_ERR;
}

#endif /* __DRIVERS_EEPROM_H__ */
