#pragma once

#include <error.hpp>

#include "app_context.hpp"
#include "task.h"

namespace app::tasks::can_rx {

[[nodiscard]] TaskHandle_t create_task(AppContext& context) noexcept;
[[nodiscard]] drivers::Result<void> connect_receive_event() noexcept;

} // namespace app::tasks::can_rx
