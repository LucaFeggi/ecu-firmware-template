#include "tasks/can_rx/can_rx_task.hpp"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"

#include <can.hpp>

#include "freertos_task.hpp"
#include "hardware.hpp"

namespace app::tasks::can_rx {
namespace {

constexpr UBaseType_t priority = 5U;
constexpr std::size_t stack_elements = 768U;
constexpr TickType_t health_check_period = pdMS_TO_TICKS(50U);
static_assert(health_check_period > 0U);

StaticTask_t task_control_block{};
StackType_t task_stack[stack_elements]{};

struct IsrContext final {
  TaskHandle_t task{nullptr};
};

IsrContext isr_context{};

void notify_from_isr(void* raw_context) noexcept {
  auto& context = *static_cast<IsrContext*>(raw_context);
  BaseType_t higher_priority_task_woken = pdFALSE;
  if (context.task != nullptr) {
    vTaskNotifyGiveFromISR(context.task, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
  }
}

void task_entry(void* argument) noexcept {
  auto& context = *static_cast<AppContext*>(argument);
  static_cast<void>(xEventGroupWaitBits(context.start_event,
                                        AppContext::started_bit, pdFALSE,
                                        pdTRUE, portMAX_DELAY));

  for (;;) {
    static_cast<void>(ulTaskNotifyTake(pdTRUE, health_check_period));

    bool receive_failed = false;
    for (;;) {
      const auto result = hardware::vehicle_bus.receive(drivers::no_wait);
      if (!result) {
        receive_failed = result.error() != drivers::Error::no_data;
        break;
      }
      // Protocol decoding belongs here in a concrete ECU application.
      static_cast<void>(result.value());
    }

    set_fault(context, Fault::can_receive, receive_failed);
    report_heartbeat(context, TaskHeartbeat::can_receive);
  }
}

} // namespace

TaskHandle_t create_task(AppContext& context) noexcept {
  isr_context.task = freertos::create_static_task(
      task_entry, "can_rx", task_stack, &context, priority, task_control_block);
  return isr_context.task;
}

drivers::Result<void> connect_receive_event() noexcept {
  if (isr_context.task == nullptr) {
    return drivers::Result<void>::failure(drivers::Error::not_initialized);
  }
  return hardware::connect_vehicle_bus_receive_event(notify_from_isr,
                                                     &isr_context);
}

} // namespace app::tasks::can_rx
