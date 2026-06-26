/**
 * @file drivers/eeprom.h
 * @brief Sim I2C EEPROM driver API (file-backed, implemented in
 *        sim_ext_storage.c). Signatures match the firmware driver:
 *        (dev, offset, data, len). Only used when the EXT_MEM backend is on.
 */

#ifndef __DRIVERS_EEPROM_H__
#define __DRIVERS_EEPROM_H__

#include "def.h"

struct device; /* opaque handle, concrete in sim_ext_storage.c */

int eeprom_read(const struct device *dev, U32 offset, U8 *buf, U32 len);
int eeprom_write(const struct device *dev, U32 offset, U8 *buf, U32 len);

#endif /* __DRIVERS_EEPROM_H__ */
