#pragma once

#include <atomic>
#include <cstdint>

namespace target::test {

enum class Test : std::uint8_t {
  adc,
  block_device,
  can_receive,
  can_transmit,
  can_fd_receive,
  can_fd_transmit,
  ethernet_receive,
  ethernet_transmit,
  gpio_input,
  gpio_output,
  i2c,
  nv_memory,
  pwm,
  rtc,
  serial_receive,
  serial_transmit,
  spi,
  watchdog,
  count
};

struct Context final {
  std::atomic<std::uint32_t> check_ins{0U};
  std::atomic<std::uint32_t> passed{0U};
  std::atomic<std::uint32_t> failed{0U};
};

[[nodiscard]] Context& context() noexcept;
[[nodiscard]] constexpr std::uint32_t bit(Test test) noexcept {
  return 1UL << static_cast<std::uint32_t>(test);
}
[[nodiscard]] constexpr std::uint32_t all_tests_mask() noexcept {
  return (1UL << static_cast<std::uint32_t>(Test::count)) - 1U;
}
void check_in(Test test) noexcept;
void record(Test test, bool success) noexcept;

} // namespace target::test
