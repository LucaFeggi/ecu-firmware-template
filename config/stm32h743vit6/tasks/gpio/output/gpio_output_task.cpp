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
    if (hardware::ready(hardware::Driver::gpio_output)) {
      hardware::gpio_output().toggle();
      test::record(test::Test::gpio_output, true);
    } else {
      test::record(test::Test::gpio_output, false);
    }
    test::check_in(test::Test::gpio_output);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500U));
  }
}

} // namespace

bool create_gpio_output_task() noexcept {
  return task_storage.create(task_entry, "gpio_out", detail::test_priority);
}

} // namespace target::tasks
