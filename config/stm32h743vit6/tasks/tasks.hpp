#pragma once

namespace target::tasks {

[[nodiscard]] bool create_adc_task() noexcept;
[[nodiscard]] bool create_block_device_task() noexcept;
[[nodiscard]] bool create_can_rx_task() noexcept;
[[nodiscard]] bool create_can_tx_task() noexcept;
[[nodiscard]] bool create_can_fd_rx_task() noexcept;
[[nodiscard]] bool create_can_fd_tx_task() noexcept;
[[nodiscard]] bool create_ethernet_rx_task() noexcept;
[[nodiscard]] bool create_ethernet_tx_task() noexcept;
[[nodiscard]] bool create_gpio_input_task() noexcept;
[[nodiscard]] bool create_gpio_output_task() noexcept;
[[nodiscard]] bool create_i2c_task() noexcept;
[[nodiscard]] bool create_nv_memory_task() noexcept;
[[nodiscard]] bool create_pwm_task() noexcept;
[[nodiscard]] bool create_rtc_task() noexcept;
[[nodiscard]] bool create_serial_rx_task() noexcept;
[[nodiscard]] bool create_serial_tx_task() noexcept;
[[nodiscard]] bool create_spi_task() noexcept;
[[nodiscard]] bool create_watchdog_task() noexcept;

} // namespace target::tasks
