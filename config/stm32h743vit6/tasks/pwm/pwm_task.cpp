#include "tasks/tasks.hpp"

#include "hardware.hpp"
#include "tasks/task_common.hpp"
#include "test_context.hpp"

#include <array>

namespace target::tasks {
namespace {

detail::TaskStorage<192U> task_storage{};
constexpr std::array<drivers::DutyCycle, 3U> duty_cycle{{
    {16'384U},
    {32'768U},
    {49'151U},
}};

void task_entry(void*) noexcept {
  for (;;) {
    if (!hardware::ready(hardware::Driver::pwm)) {
      test::record(test::Test::pwm, false);
      test::check_in(test::Test::pwm);
      vTaskDelay(pdMS_TO_TICKS(500U));
      continue;
    }
    hardware::pwm().enable();
    for (const auto duty : duty_cycle) {
      hardware::pwm().set_duty(duty);
      test::record(test::Test::pwm, true);
      test::check_in(test::Test::pwm);
      vTaskDelay(pdMS_TO_TICKS(250U));
    }
    hardware::pwm().disable();
    test::check_in(test::Test::pwm);
    vTaskDelay(pdMS_TO_TICKS(250U));
  }
}

} // namespace

bool create_pwm_task() noexcept {
  return task_storage.create(task_entry, "pwm", detail::test_priority);
}

} // namespace target::tasks
