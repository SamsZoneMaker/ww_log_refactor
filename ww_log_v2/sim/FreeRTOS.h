/**
 * @file FreeRTOS.h
 * @brief Sim stub: minimal FreeRTOS type definitions.
 *
 * The sim is single-threaded.  Semaphore / mutex operations in
 * n_ww_log_task_sim.c are no-ops.  These types only need to compile.
 */

#ifndef __FREERTOS_H__
#define __FREERTOS_H__

#include "def.h"

/* FreeRTOS primitive types */
typedef void *  TaskHandle_t;
typedef void *  SemaphoreHandle_t;
typedef void *  TimerHandle_t;
typedef U32     TickType_t;
typedef S32     BaseType_t;
typedef U32     UBaseType_t;

#define pdTRUE          ((BaseType_t)1)
#define pdFALSE         ((BaseType_t)0)
#define pdPASS          pdTRUE
#define pdFAIL          pdFALSE

#define portMAX_DELAY   ((TickType_t)0xFFFFFFFF)

static inline TickType_t pdMS_TO_TICKS(U32 ms) { return (TickType_t)ms; }

#endif /* __FREERTOS_H__ */
