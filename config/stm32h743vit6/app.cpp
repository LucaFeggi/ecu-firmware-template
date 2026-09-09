#include "app.hpp"

#include "hardware.hpp"
#include "tasks/tasks.hpp"

namespace app {

bool initialize() noexcept {
  static_cast<void>(target::hardware::initialize());

  return target::tasks::create_adc_task() &&
         target::tasks::create_block_device_task() &&
         target::tasks::create_can_rx_task() &&
         target::tasks::create_can_tx_task() &&
         target::tasks::create_can_fd_rx_task() &&
         target::tasks::create_can_fd_tx_task() &&
         target::tasks::create_ethernet_rx_task() &&
         target::tasks::create_ethernet_tx_task() &&
         target::tasks::create_gpio_input_task() &&
         target::tasks::create_gpio_output_task() &&
         target::tasks::create_i2c_task() &&
         target::tasks::create_nv_memory_task() &&
         target::tasks::create_pwm_task() &&
         target::tasks::create_rtc_task() &&
         target::tasks::create_serial_rx_task() &&
         target::tasks::create_serial_tx_task() &&
         target::tasks::create_spi_task() &&
         target::tasks::create_watchdog_task();
}

} // namespace app
