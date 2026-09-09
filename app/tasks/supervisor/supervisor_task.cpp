#include "tasks/supervisor/supervisor_task.hpp"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"

#include <chrono>
#include <cstddef>

#include "freertos_task.hpp"
#include "hardware.hpp"

namespace app::tasks::supervisor {
namespace {

constexpr UBaseType_t priority = 6U;
constexpr std::size_t stack_elements = 512U;
constexpr TickType_t supervision_period = pdMS_TO_TICKS(200U);
constexpr auto watchdog_timeout = std::chrono::milliseconds{750};
static_assert(supervision_period > 0U);

StaticTask_t task_control_block{};
StackType_t task_stack[stack_elements]{};

[[noreturn]] void startup_failed(AppContext& context) noexcept {
  context.health_working.active_faults |=
      fault_mask(Fault::application_initialization);
  publish_health_snapshot(context);
  hardware::cooling_fan.disable();
  hardware::status_led.set_active(true);
  for (;;) {
    vTaskDelay(supervision_period);
  }
}

void task_entry(void* argument) noexcept {
  auto& context = *static_cast<AppContext*>(argument);

  const auto interrupt_result = hardware::enable_application_interrupts();
  if (!interrupt_result) {
    startup_failed(context);
  }
  const auto watchdog_result = hardware::watchdog.start(watchdog_timeout);
  if (!watchdog_result) {
    startup_failed(context);
  }

  static_cast<void>(
      xEventGroupSetBits(context.start_event, AppContext::started_bit));
  TickType_t last_wake = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&last_wake, supervision_period);

    const auto seen =
        context.heartbeats.exchange(0U, std::memory_order_relaxed);
    const auto missing = context.required_heartbeats & ~seen;
    context.health_working.missing_heartbeats = missing;
    set_fault(context, Fault::missing_heartbeat, missing != 0U);
    publish_health_snapshot(context);
    const auto health = read_health_snapshot(context);

    if (missing == 0U && health.active_faults == 0U) {
      hardware::watchdog.refresh();
    } else {
      hardware::cooling_fan.disable();
    }

#if defined(ECU_HOST_TEST_DURATION_TICKS)
    if (xTaskGetTickCount() >=
        static_cast<TickType_t>(ECU_HOST_TEST_DURATION_TICKS)) {
      vTaskEndScheduler();
    }
#endif
  }
}

} // namespace

TaskHandle_t create_task(AppContext& context) noexcept {
  return freertos::create_static_task(task_entry, "supervisor", task_stack,
                                      &context, priority, task_control_block);
}

} // namespace app::tasks::supervisor
