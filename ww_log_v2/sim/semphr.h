/**
 * @file semphr.h
 * @brief Sim stub: FreeRTOS semaphore/mutex API (single-threaded no-ops).
 */

#ifndef __SEMPHR_H__
#define __SEMPHR_H__

#include "FreeRTOS.h"

static inline SemaphoreHandle_t xSemaphoreCreateBinary(void)   { return (SemaphoreHandle_t)1; }
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void)    { return (SemaphoreHandle_t)1; }
static inline void vSemaphoreDelete(SemaphoreHandle_t s)        { (void)s; }

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t)
{
    (void)s; (void)t;
    return pdTRUE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    (void)s;
    return pdTRUE;
}

#endif /* __SEMPHR_H__ */
