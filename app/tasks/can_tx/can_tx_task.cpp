#include "tasks/can_tx/can_tx_task.hpp"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"

#include <chrono>

#include "freertos_task.hpp"
#include "hardware.hpp"

namespace app::tasks::can_tx {
namespace {

constexpr UBaseType_t priority = 3U;
constexpr std::size_t stack_elements = 512U;
constexpr TickType_t health_check_period = pdMS_TO_TICKS(50U);
constexpr drivers::Timeout transmit_timeout = std::chrono::milliseconds{2};
static_assert(health_check_period > 0U);

StaticTask_t task_control_block{};
StackType_t task_stack[stack_elements]{};

void task_entry(void* argument) noexcept {
  auto& context = *static_cast<AppContext*>(argument);
  static_cast<void>(xEventGroupWaitBits(context.start_event,
                                        AppContext::started_bit, pdFALSE,
                                        pdTRUE, portMAX_DELAY));

  for (;;) {
    drivers::CanFrame frame{};
    const auto received =
        xQueueReceive(context.can_tx_queue, &frame, health_check_period);
    bool transmit_failed = false;
    if (received == pdPASS) {
      const auto result = hardware::vehicle_bus.send(frame, transmit_timeout);
      transmit_failed = !result;
    }
    set_fault(context, Fault::can_transmit, transmit_failed);
    report_heartbeat(context, TaskHeartbeat::can_transmit);
  }
}

} // namespace

TaskHandle_t create_task(AppContext& context) noexcept {
  return freertos::create_static_task(task_entry, "can_tx", task_stack,
                                      &context, priority, task_control_block);
}

bool enqueue(AppContext& context, const drivers::CanFrame& frame,
             TickType_t wait_ticks) noexcept {
  return xQueueSend(context.can_tx_queue, &frame, wait_ticks) == pdPASS;
}

} // namespace app::tasks::can_tx
