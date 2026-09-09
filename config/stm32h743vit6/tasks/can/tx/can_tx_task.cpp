#include "tasks/tasks.hpp"

#include "hardware.hpp"
#include "tasks/task_common.hpp"
#include "test_context.hpp"

#include <chrono>

namespace target::tasks {
namespace {

detail::TaskStorage<256U> task_storage{};

void task_entry(void*) noexcept {
  std::uint32_t sequence = 0U;
  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    bool passed = false;
    if (hardware::ready(hardware::Driver::can)) {
      drivers::CanFrame frame{};
      frame.identifier = 0x321U;
      frame.format = drivers::CanIdFormat::standard;
      frame.size = 8U;
      for (std::size_t index = 0U; index < frame.data.size(); ++index) {
        frame.data[index] = static_cast<std::byte>(
            (sequence >> ((index % 4U) * 8U)) & 0xFFU);
      }
      passed = static_cast<bool>(
          hardware::can().send(frame, std::chrono::milliseconds{5}));
      ++sequence;
    }
    test::record(test::Test::can_transmit, passed);
    test::check_in(test::Test::can_transmit);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500U));
  }
}

} // namespace

bool create_can_tx_task() noexcept {
  return task_storage.create(task_entry, "can_tx", detail::io_priority);
}

} // namespace target::tasks
