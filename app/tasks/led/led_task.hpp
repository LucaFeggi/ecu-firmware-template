#pragma once

#include "app_context.hpp"
#include "task.h"

namespace app::tasks::led {

[[nodiscard]] TaskHandle_t create_task(AppContext& context) noexcept;

} // namespace app::tasks::led
