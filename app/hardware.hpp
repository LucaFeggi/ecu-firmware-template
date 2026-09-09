#pragma once

#include <adc.hpp>
#include <can.hpp>
#include <error.hpp>
#include <gpio.hpp>
#include <pwm.hpp>
#include <serial.hpp>
#include <watchdog.hpp>

namespace hardware {

extern drivers::Can vehicle_bus;
extern drivers::Adc accelerator_primary;
extern drivers::Adc accelerator_secondary;
extern drivers::Pwm cooling_fan;
extern drivers::DigitalOutput status_led;
extern drivers::Serial diagnostics;
extern drivers::Watchdog watchdog;

[[nodiscard]] drivers::Result<void> initialize() noexcept;

using IsrEventCallback = void (*)(void* context) noexcept;
[[nodiscard]] drivers::Result<void>
connect_vehicle_bus_receive_event(IsrEventCallback callback,
                                  void* context) noexcept;
[[nodiscard]] drivers::Result<void> enable_application_interrupts() noexcept;

} // namespace hardware
