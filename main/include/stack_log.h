#ifndef STACK_LOG_H
#define STACK_LOG_H

/*********************
 *      INCLUDES
 *********************/
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>
#include <esp_heap_caps.h>

/*********************
 *      DEFINES
 *********************/
#define LOG_STACK_USAGE 1
#define LOG_HEAP_USAGE 0

/**********************
 *      MACROS
 **********************/
#if LOG_STACK_USAGE || LOG_HEAP_USAGE
#define LOG_STACK_USAGE_TASK_INIT(task_name)        \
    UBaseType_t stackHighWaterMark_##task_name = 0; \
    uint32_t heap_size_##task_name = 0;

#define LOG_STACK_USAGE_TASK(xTask, task_name)                                                                                                                                                              \
    do                                                                                                                                                                                                      \
    {                                                                                                                                                                                                       \
        UBaseType_t ___stackHighWaterMark = uxTaskGetStackHighWaterMark(xTask);                                                                                                                             \
        uint32_t ___heap_size = esp_get_free_heap_size();                                                                                                                                                   \
        if ((LOG_STACK_USAGE && ___stackHighWaterMark != stackHighWaterMark_##task_name) || (LOG_HEAP_USAGE && ___heap_size != heap_size_##task_name))                                                      \
        {                                                                                                                                                                                                   \
            stackHighWaterMark_##task_name = ___stackHighWaterMark;                                                                                                                                         \
            heap_size_##task_name = ___heap_size;                                                                                                                                                           \
            ESP_LOGW("MEM USAGE", "Task %s remaining stack size: %u bytes, free heap size: %" PRIu32 " bytes", #task_name, stackHighWaterMark_##task_name * sizeof(StackType_t), esp_get_free_heap_size()); \
        }                                                                                                                                                                                                   \
        if (false && xTask == NULL && !heap_caps_check_integrity_all(true))                                                                                                                                                           \
        {                                                                                                                                                                                                   \
            ESP_LOGE("CAP CHECK", "Heap integrity check failed");                                                                                                                                           \
        }                                                                                                                                                                                                   \
    } while (0);

#else
#define LOG_STACK_USAGE_TASK_INIT(task_name) ;
#define LOG_STACK_USAGE_TASK(xTask, task_name) ;
#endif

#endif // STACK_LOG_H
