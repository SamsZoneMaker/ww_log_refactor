/**
 * @file timers.h
 * @brief Sim stub: FreeRTOS software timers (unused in sim, compile-only).
 */

#ifndef __TIMERS_H__
#define __TIMERS_H__

#include "FreeRTOS.h"

typedef void (*TimerCallbackFunction_t)(TimerHandle_t xTimer);

static inline TimerHandle_t xTimerCreate(const char *name, TickType_t period,
                                         UBaseType_t reload, void *id,
                                         TimerCallbackFunction_t cb)
{
    (void)name; (void)period; (void)reload; (void)id; (void)cb;
    return (TimerHandle_t)0;
}

#endif /* __TIMERS_H__ */
