#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"

#include <adc.hpp>
#include <can.hpp>

#include "system_fault.hpp"

namespace app {

enum class TaskHeartbeat : std::uint8_t {
  can_receive,
  sensing,
  can_transmit,
  status_led,
};

[[nodiscard]] constexpr std::uint32_t
heartbeat_mask(TaskHeartbeat heartbeat) noexcept {
  return 1UL << static_cast<std::uint8_t>(heartbeat);
}

struct SensorSnapshot final {
  drivers::AdcSample accelerator_primary{0U, 1U, 0U};
  drivers::AdcSample accelerator_secondary{0U, 1U, 0U};
};

struct HealthSnapshot final {
  std::uint32_t active_faults{0U};
  std::uint32_t missing_heartbeats{0U};
};

struct AppContext final {
  static constexpr EventBits_t started_bit = 1U << 0U;
  static constexpr std::size_t can_tx_capacity = 16U;

  StaticEventGroup_t start_event_storage{};
  EventGroupHandle_t start_event{nullptr};

  StaticQueue_t can_tx_queue_storage{};
  alignas(drivers::CanFrame) std::array<
      std::byte, sizeof(drivers::CanFrame) * can_tx_capacity> can_tx_storage{};
  QueueHandle_t can_tx_queue{nullptr};

  SensorSnapshot sensor_working{};
  SensorSnapshot sensor_published{};
  HealthSnapshot health_working{};
  HealthSnapshot health_published{};

  std::atomic<std::uint32_t> heartbeats{0U};
  std::uint32_t required_heartbeats{0U};
};

static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

inline void report_heartbeat(AppContext& context,
                             TaskHeartbeat heartbeat) noexcept {
  context.heartbeats.fetch_or(heartbeat_mask(heartbeat),
                              std::memory_order_relaxed);
}

inline void set_fault(AppContext& context, Fault fault, bool active) noexcept {
  const auto mask = fault_mask(fault);
  taskENTER_CRITICAL();
  if (active) {
    context.health_working.active_faults |= mask;
  } else {
    context.health_working.active_faults &= ~mask;
  }
  taskEXIT_CRITICAL();
}

inline void publish_sensor_snapshot(AppContext& context) noexcept {
  taskENTER_CRITICAL();
  context.sensor_published = context.sensor_working;
  taskEXIT_CRITICAL();
}

[[nodiscard]] inline SensorSnapshot
read_sensor_snapshot(const AppContext& context) noexcept {
  SensorSnapshot snapshot{};
  taskENTER_CRITICAL();
  snapshot = context.sensor_published;
  taskEXIT_CRITICAL();
  return snapshot;
}

inline void publish_health_snapshot(AppContext& context) noexcept {
  taskENTER_CRITICAL();
  context.health_published = context.health_working;
  taskEXIT_CRITICAL();
}

[[nodiscard]] inline HealthSnapshot
read_health_snapshot(const AppContext& context) noexcept {
  HealthSnapshot snapshot{};
  taskENTER_CRITICAL();
  snapshot = context.health_published;
  taskEXIT_CRITICAL();
  return snapshot;
}

} // namespace app
