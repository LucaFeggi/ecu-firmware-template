#pragma once

#include "freertos_task.hpp"

#include "FreeRTOS.h"
#include "task.h"

#include <cstddef>

namespace target::tasks::detail {

inline constexpr UBaseType_t test_priority = 2U;
inline constexpr UBaseType_t io_priority = 3U;
inline constexpr UBaseType_t supervisor_priority = 6U;

template <std::size_t StackElements> struct TaskStorage final {
  StaticTask_t control_block{};
  StackType_t stack[StackElements]{};
  TaskHandle_t handle{nullptr};

  [[nodiscard]] bool create(TaskFunction_t entry, const char* name,
                            UBaseType_t priority,
                            void* context = nullptr) noexcept {
    handle = app::freertos::create_static_task(
        entry, name, stack, context, priority, control_block);
    return handle != nullptr;
  }
};

} // namespace target::tasks::detail
