#include "app.hpp"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"

#include <cstdint>

#include "tasks/can_rx/can_rx_task.hpp"
#include "tasks/can_tx/can_tx_task.hpp"
#include "tasks/led/led_task.hpp"
#include "tasks/sensing/sensing_task.hpp"
#include "tasks/supervisor/supervisor_task.hpp"

namespace app {
namespace {

AppContext application_context{};

} // namespace

AppContext& context() noexcept { return application_context; }

bool initialize() noexcept {
  auto& app_context = context();

  app_context.start_event =
      xEventGroupCreateStatic(&app_context.start_event_storage);
  if (app_context.start_event == nullptr) {
    return false;
  }

  app_context.can_tx_queue = xQueueCreateStatic(
      static_cast<UBaseType_t>(AppContext::can_tx_capacity),
      static_cast<UBaseType_t>(sizeof(drivers::CanFrame)),
      reinterpret_cast<std::uint8_t*>(app_context.can_tx_storage.data()),
      &app_context.can_tx_queue_storage);
  if (app_context.can_tx_queue == nullptr) {
    return false;
  }

  publish_sensor_snapshot(app_context);
  publish_health_snapshot(app_context);

  const auto can_rx_handle = tasks::can_rx::create_task(app_context);
  const auto sensing_handle = tasks::sensing::create_task(app_context);
  const auto can_tx_handle = tasks::can_tx::create_task(app_context);
  const auto led_handle = tasks::led::create_task(app_context);
  if (can_rx_handle == nullptr || sensing_handle == nullptr ||
      can_tx_handle == nullptr || led_handle == nullptr) {
    return false;
  }

  app_context.required_heartbeats =
      heartbeat_mask(TaskHeartbeat::can_receive) |
      heartbeat_mask(TaskHeartbeat::sensing) |
      heartbeat_mask(TaskHeartbeat::can_transmit) |
      heartbeat_mask(TaskHeartbeat::status_led);

  if (!tasks::can_rx::connect_receive_event()) {
    return false;
  }

  // The supervisor is created last. It is the only task not initially blocked
  // on start_event and owns the final interrupt/watchdog/startup gate.
  return tasks::supervisor::create_task(app_context) != nullptr;
}

} // namespace app
