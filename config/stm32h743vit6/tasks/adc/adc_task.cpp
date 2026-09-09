#include "tasks/tasks.hpp"

#include "hardware.hpp"
#include "tasks/task_common.hpp"
#include "test_context.hpp"

namespace target::tasks {
namespace {

detail::TaskStorage<256U> task_storage{};

void task_entry(void*) noexcept {
  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    bool passed = false;
    if (hardware::ready(hardware::Driver::adc)) {
      const auto sample = hardware::adc().latest();
      if (sample) {
        const auto& value = sample.value();
        passed = value.sample_count != 0U &&
                 value.accumulated_raw <=
                     hardware::adc().full_scale() * value.sample_count;
      }
    }
    test::record(test::Test::adc, passed);
    test::check_in(test::Test::adc);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100U));
  }
}

} // namespace

bool create_adc_task() noexcept {
  return task_storage.create(task_entry, "adc", detail::test_priority);
}

} // namespace target::tasks
