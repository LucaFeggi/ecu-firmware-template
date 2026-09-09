#pragma once

#include <cstdint>

#include "FreeRTOS.h"

namespace app {

[[nodiscard]] constexpr bool
valid_frequency(std::uint32_t frequency_hz) noexcept {
  return frequency_hz > 0U && (configTICK_RATE_HZ / frequency_hz) > 0U;
}

[[nodiscard]] constexpr TickType_t
period_ticks(std::uint32_t frequency_hz) noexcept {
  return static_cast<TickType_t>(configTICK_RATE_HZ / frequency_hz);
}

} // namespace app
