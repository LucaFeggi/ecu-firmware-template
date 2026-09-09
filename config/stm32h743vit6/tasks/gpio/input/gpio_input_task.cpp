#include "tasks/tasks.hpp"

#include "hardware.hpp"
#include "tasks/task_common.hpp"
#include "test_context.hpp"

namespace target::tasks {
namespace {

detail::TaskStorage<192U> task_storage{};

void task_entry(void*) noexcept {
  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    if (!hardware::ready(hardware::Driver::gpio_input)) {
      test::record(test::Test::gpio_input, false);
    } else if (hardware::gpio_input().is_active()) {
      test::record(test::Test::gpio_input, true);
    }
    test::check_in(test::Test::gpio_input);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(20U));
  }
}

} // namespace

bool create_gpio_input_task() noexcept {
  return task_storage.create(task_entry, "gpio_in", detail::test_priority);
}

} // namespace target::tasks
