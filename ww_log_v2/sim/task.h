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

#endif /* __TASK_H__ */
