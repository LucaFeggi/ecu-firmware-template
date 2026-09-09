#include "tasks/tasks.hpp"

#include "hardware.hpp"
#include "tasks/task_common.hpp"
#include "test_context.hpp"

#include <chrono>

namespace target::tasks {
namespace {

detail::TaskStorage<320U> task_storage{};

void task_entry(void*) noexcept {
  std::uint32_t sequence = 0U;
  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    bool passed = false;
    if (hardware::ready(hardware::Driver::can_fd)) {
      drivers::CanFdFrame frame{};
      frame.identifier = 0x18FF50E5U;
      frame.format = drivers::CanIdFormat::extended;
      frame.size = 16U;
      frame.bit_rate_switch = true;
      for (std::size_t index = 0U; index < frame.size; ++index) {
        frame.data[index] = static_cast<std::byte>(
            (sequence + static_cast<std::uint32_t>(index)) & 0xFFU);
      }
      passed = static_cast<bool>(
          hardware::can_fd().send(frame, std::chrono::milliseconds{5}));
      ++sequence;
    }
    test::record(test::Test::can_fd_transmit, passed);
    test::check_in(test::Test::can_fd_transmit);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500U));
  }
}

} // namespace

bool create_can_fd_tx_task() noexcept {
  return task_storage.create(task_entry, "canfd_tx", detail::io_priority);
}

} // namespace target::tasks
