#include "tasks/tasks.hpp"

#include "hardware.hpp"
#include "tasks/task_common.hpp"
#include "test_context.hpp"

#include "FreeRTOS.h"
#include "task.h"

namespace target::tasks {
namespace {

detail::TaskStorage<384U> task_storage{};
bool notifications_ready{false};

void receive_event(void* context) noexcept {
  auto* const handle = static_cast<TaskHandle_t*>(context);
  BaseType_t higher_priority_woken = pdFALSE;
  if (handle != nullptr && *handle != nullptr) {
    vTaskNotifyGiveFromISR(*handle, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
  }
}

void task_entry(void*) noexcept {
  for (;;) {
    if (!hardware::ready(hardware::Driver::can_fd) || !notifications_ready) {
      test::record(test::Test::can_fd_receive, false);
      test::check_in(test::Test::can_fd_receive);
      vTaskDelay(pdMS_TO_TICKS(250U));
      continue;
    }
    static_cast<void>(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250U)));
    for (;;) {
      const auto result = hardware::can_fd().receive(drivers::no_wait);
      if (!result) {
        break;
      }
      const auto& frame = result.value();
      test::record(test::Test::can_fd_receive,
                   frame.identifier == 0x18FF50E6U &&
                       frame.format == drivers::CanIdFormat::extended &&
                       frame.size == 16U && frame.bit_rate_switch);
    }
    test::check_in(test::Test::can_fd_receive);
  }
}

} // namespace

bool create_can_fd_rx_task() noexcept {
  if (!task_storage.create(task_entry, "canfd_rx", detail::io_priority)) {
    return false;
  }
  notifications_ready = hardware::connect_can_fd_receive_event(
      receive_event, &task_storage.handle);
  return true;
}

} // namespace target::tasks
