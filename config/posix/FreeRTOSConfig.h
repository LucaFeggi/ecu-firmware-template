#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void vAssertCalled(const char* file, unsigned long line);
#ifdef __cplusplus
}
#endif

#define configASSERT(expression)                                                 \
    do {                                                                         \
        if ((expression) == 0) {                                                 \
            vAssertCalled(__FILE__, (unsigned long)__LINE__);                    \
        }                                                                        \
    } while (0)

#define configCPU_CLOCK_HZ ((unsigned long)1000000000UL)
#define configTICK_RATE_HZ ((TickType_t)1000U)
#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_TIME_SLICING 1
#define configUSE_TICKLESS_IDLE 0
#define configMAX_PRIORITIES 7
#define configMINIMAL_STACK_SIZE 256U
#define configMAX_TASK_NAME_LEN 24
#define configUSE_16_BIT_TICKS 0
#define configSTACK_DEPTH_TYPE uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE size_t

#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 0

#define configUSE_TASK_NOTIFICATIONS 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1
#define configUSE_MUTEXES 0
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 0
#define configUSE_QUEUE_SETS 0
#define configUSE_TIMERS 0
#define configUSE_CO_ROUTINES 0

#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0
#define configUSE_NEWLIB_REENTRANT 0
#define configENABLE_BACKWARD_COMPATIBILITY 0
#define configQUEUE_REGISTRY_SIZE 0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#define configGENERATE_RUN_TIME_STATS 0
#define configUSE_TRACE_FACILITY 0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0

#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
