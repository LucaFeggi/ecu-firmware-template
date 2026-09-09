#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "FreeRTOS.h"
#include "task.h"

extern "C" {

void vApplicationGetIdleTaskMemory(StaticTask_t** task_control_block,
                                   StackType_t** stack, uint32_t* stack_size) {
  static StaticTask_t idle_control_block{};
  static StackType_t idle_stack[configMINIMAL_STACK_SIZE]{};
  *task_control_block = &idle_control_block;
  *stack = idle_stack;
  *stack_size = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t, char*) { std::abort(); }

void vAssertCalled(const char* file, unsigned long line) {
  std::fprintf(stderr, "FreeRTOS assertion failed: %s:%lu\n", file, line);
  std::abort();
}

void vLoggingPrintf(const char* format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
}

} // extern "C"
