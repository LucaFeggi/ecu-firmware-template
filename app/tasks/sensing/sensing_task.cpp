#include "tasks/sensing/sensing_task.hpp"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"

#include "freertos_task.hpp"
#include "hardware.hpp"

namespace app::tasks::sensing {
namespace {

constexpr UBaseType_t priority = 4U;
constexpr std::size_t stack_elements = 512U;
constexpr TickType_t period = pdMS_TO_TICKS(5U);
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
    const auto primary = hardware::accelerator_primary.latest();
    const auto secondary = hardware::accelerator_secondary.latest();

    set_fault(context, Fault::adc_primary, !primary);
    set_fault(context, Fault::adc_secondary, !secondary);
    if (primary && secondary) {
      context.sensor_working.accelerator_primary = primary.value();
      context.sensor_working.accelerator_secondary = secondary.value();
      publish_sensor_snapshot(context);
    }

    report_heartbeat(context, TaskHeartbeat::sensing);
    vTaskDelayUntil(&last_wake, period);
  }
}

} // namespace

TaskHandle_t create_task(AppContext& context) noexcept {
  return freertos::create_static_task(task_entry, "sensing", task_stack,
                                      &context, priority, task_control_block);
}

} // namespace app::tasks::sensing
