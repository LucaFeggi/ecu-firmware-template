#pragma once

#include <cstdint>

#include <adc.hpp>
#include <block_device.hpp>
#include <can.hpp>
#include <can_fd.hpp>
#include <ethernet_mac.hpp>
#include <gpio.hpp>
#include <i2c.hpp>
#include <nv_memory.hpp>
#include <pwm.hpp>
#include <rtc.hpp>
#include <serial.hpp>
#include <spi.hpp>
#include <watchdog.hpp>

namespace target::hardware {

enum class Driver : std::uint8_t {
  adc,
  block_device,
  can,
  can_fd,
  ethernet,
  gpio_input,
  gpio_output,
  i2c,
  nv_memory,
  pwm,
  rtc,
  serial,
  spi,
  watchdog,
  count
};

[[nodiscard]] bool initialize() noexcept;
[[nodiscard]] bool ready(Driver driver) noexcept;
[[nodiscard]] std::uint32_t ready_mask() noexcept;

[[nodiscard]] drivers::Adc& adc() noexcept;
[[nodiscard]] drivers::BlockDevice& block_device() noexcept;
[[nodiscard]] drivers::Can& can() noexcept;
[[nodiscard]] drivers::CanFd& can_fd() noexcept;
[[nodiscard]] drivers::EthernetMac& ethernet() noexcept;
[[nodiscard]] drivers::DigitalInput& gpio_input() noexcept;
[[nodiscard]] drivers::DigitalOutput& gpio_output() noexcept;
[[nodiscard]] drivers::I2cDevice& i2c() noexcept;
[[nodiscard]] drivers::NvMemory& nv_memory() noexcept;
[[nodiscard]] drivers::Pwm& pwm() noexcept;
[[nodiscard]] drivers::Rtc& rtc() noexcept;
[[nodiscard]] drivers::Serial& serial() noexcept;
[[nodiscard]] drivers::SpiDevice& spi() noexcept;
[[nodiscard]] drivers::Watchdog& watchdog() noexcept;

void handle_adc_irq() noexcept;
void handle_can_irq() noexcept;
void handle_can_fd_irq() noexcept;
[[nodiscard]] bool connect_can_receive_event(
    void (*callback)(void*) noexcept, void* context) noexcept;
[[nodiscard]] bool connect_can_fd_receive_event(
    void (*callback)(void*) noexcept, void* context) noexcept;

} // namespace target::hardware

namespace target {

[[nodiscard]] std::uint64_t monotonic_microseconds() noexcept;

} // namespace target
