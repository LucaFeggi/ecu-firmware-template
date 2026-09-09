#pragma once

#include "app_context.hpp"
#include "task.h"

namespace app::tasks::supervisor {

[[nodiscard]] TaskHandle_t create_task(AppContext& context) noexcept;

} // namespace app::tasks::supervisor
