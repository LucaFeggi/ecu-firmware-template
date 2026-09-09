#include "tasks/led/led_task.hpp"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"

#include "freertos_task.hpp"
#include "hardware.hpp"

namespace app::tasks::led {
namespace {

constexpr UBaseType_t priority = 1U;
constexpr std::size_t stack_elements = 384U;
constexpr TickType_t period = pdMS_TO_TICKS(100U);
static_assert(period > 0U);

StaticTask_t task_control_block{};
StackType_t task_stack[stack_elements]{};

void task_entry(void* argument) noexcept {
  auto& context = *static_cast<AppContext*>(argument);
  static_cast<void>(xEventGroupWaitBits(context.start_event,
                                        AppContext::started_bit, pdFALSE,
                                        pdTRUE, portMAX_DELAY));

  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    const auto health = read_health_snapshot(context);
    if (health.active_faults == 0U) {
      hardware::status_led.toggle();
    } else {
      hardware::status_led.set_active(true);
    }
    report_heartbeat(context, TaskHeartbeat::status_led);
    vTaskDelayUntil(&last_wake, period);
  }
}

} // namespace

TaskHandle_t create_task(AppContext& context) noexcept {
  return freertos::create_static_task(task_entry, "status_led", task_stack,
                                      &context, priority, task_control_block);
}

} // namespace app::tasks::led
