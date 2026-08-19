/**
 * @file task.h
 * @brief Sim stub: FreeRTOS task API.
 */

#ifndef __TASK_H__
#define __TASK_H__

#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void *pvParameters);

static inline BaseType_t xTaskCreate(TaskFunction_t fn, const char *name,
                                     U16 stack, void *params,
                                     UBaseType_t prio, TaskHandle_t *handle)
{
    (void)fn; (void)name; (void)stack; (void)params; (void)prio; (void)handle;
    return pdPASS;
}

static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }

/* Sim tick source: a monotonically increasing counter so flush markers get
 * distinct, ordered timestamps (the real target uses the FreeRTOS scheduler
 * tick). */
static inline TickType_t xTaskGetTickCount(void)
{
    static TickType_t s_sim_ticks = 0;
    return ++s_sim_ticks;
}

#endif /* __TASK_H__ */
