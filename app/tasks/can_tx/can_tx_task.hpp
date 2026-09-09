#pragma once

#include <can.hpp>

#include "app_context.hpp"
#include "task.h"

namespace app::tasks::can_tx {

[[nodiscard]] TaskHandle_t create_task(AppContext& context) noexcept;
[[nodiscard]] bool enqueue(AppContext& context, const drivers::CanFrame& frame,
                           TickType_t wait_ticks = 0U) noexcept;

} // namespace app::tasks::can_tx
