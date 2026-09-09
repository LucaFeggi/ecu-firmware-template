#include "hardware.hpp"

#include <array>
#include <cstddef>

#include <backend.hpp>

namespace backend = drivers::posix;

namespace {

backend::CanStorage vehicle_bus_storage{.loopback = true};
backend::SampledAdcStorage accelerator_primary_storage{
    .full_scale = 4095U,
    .reduction = {drivers::AdcReduction::moving_mean, 4U},
};
backend::SampledAdcStorage accelerator_secondary_storage{
    .full_scale = 4095U,
    .reduction = {drivers::AdcReduction::moving_mean, 8U},
};
backend::PwmStorage cooling_fan_storage{};
backend::DigitalOutputStorage status_led_storage{};
backend::SerialStorage diagnostics_storage{};
backend::WatchdogStorage watchdog_storage{};

[[nodiscard]] drivers::Result<void> initialize_sampled_inputs() noexcept {
  if (auto result = backend::initialize(accelerator_primary_storage); !result) {
    return result;
  }
  if (auto result = backend::initialize(accelerator_secondary_storage);
      !result) {
    return result;
  }

  constexpr std::array<std::uint32_t, 8> primary_samples{
      1024U, 1024U, 1024U, 1024U, 1024U, 1024U, 1024U, 1024U};
  constexpr std::array<std::uint32_t, 8> secondary_samples{
      1030U, 1030U, 1030U, 1030U, 1030U, 1030U, 1030U, 1030U};
  for (const auto sample : primary_samples) {
    if (auto result = backend::publish_raw(accelerator_primary_storage, sample);
        !result) {
      return result;
    }
  }
  for (const auto sample : secondary_samples) {
    if (auto result =
            backend::publish_raw(accelerator_secondary_storage, sample);
        !result) {
      return result;
    }
  }
  return drivers::Result<void>::success();
}

} // namespace

namespace hardware {

drivers::Can vehicle_bus = backend::bind(vehicle_bus_storage);
drivers::SampledAdc accelerator_primary =
    backend::bind(accelerator_primary_storage);
drivers::SampledAdc accelerator_secondary =
    backend::bind(accelerator_secondary_storage);
drivers::Pwm cooling_fan = backend::bind(cooling_fan_storage);
drivers::DigitalOutput status_led = backend::bind(status_led_storage);
drivers::Serial diagnostics = backend::bind(diagnostics_storage);
drivers::Watchdog watchdog = backend::bind(watchdog_storage);

drivers::Result<void> initialize() noexcept {
  if (auto result = backend::initialize(vehicle_bus_storage); !result) {
    return result;
  }
  if (auto result = initialize_sampled_inputs(); !result) {
    return result;
  }
  if (auto result = backend::initialize(cooling_fan_storage); !result) {
    return result;
  }
  if (auto result = backend::initialize(status_led_storage); !result) {
    return result;
  }
  if (auto result = backend::initialize(diagnostics_storage); !result) {
    return result;
  }
  if (auto result = backend::initialize(watchdog_storage); !result) {
    return result;
  }

  cooling_fan.disable();
  status_led.set_active(false);
  return drivers::Result<void>::success();
}

drivers::Result<void>
connect_vehicle_bus_receive_event(IsrEventCallback callback,
                                  void* context) noexcept {
  return backend::connect_receive_event(vehicle_bus_storage, callback, context);
}

drivers::Result<void> enable_application_interrupts() noexcept {
  return backend::enable_receive_event(vehicle_bus_storage);
}

} // namespace hardware
