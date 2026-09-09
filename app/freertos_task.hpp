#pragma once

#include <cstddef>

#include "FreeRTOS.h"
#include "task.h"

namespace app::freertos {

template <std::size_t StackElements>
[[nodiscard]] TaskHandle_t
create_static_task(TaskFunction_t entry, const char* name,
                   StackType_t (&stack)[StackElements], void* context,
                   UBaseType_t priority, StaticTask_t& control_block) noexcept {
  static_assert(StackElements > 0U);
#if defined(ECU_FREERTOS_STATIC_STACK_SIZE_IN_BYTES)
  constexpr auto stack_size =
      static_cast<configSTACK_DEPTH_TYPE>(StackElements * sizeof(StackType_t));
#else
  constexpr auto stack_size =
      static_cast<configSTACK_DEPTH_TYPE>(StackElements);
#endif
  return xTaskCreateStatic(entry, name, stack_size, context, priority, stack,
                           &control_block);
}

} // namespace app::freertos
