#pragma once

#include <cstdint>

namespace app {

enum class Fault : std::uint8_t {
  hardware_initialization,
  application_initialization,
  scheduler_returned,
  adc_primary,
  adc_secondary,
  can_receive,
  can_transmit,
  missing_heartbeat,
};

[[nodiscard]] constexpr std::uint32_t fault_mask(Fault fault) noexcept {
  return 1UL << static_cast<std::uint8_t>(fault);
}

} // namespace app
