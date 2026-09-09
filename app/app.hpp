#pragma once

#include "app_context.hpp"

namespace app {

[[nodiscard]] bool initialize() noexcept;
[[nodiscard]] AppContext& context() noexcept;

} // namespace app
