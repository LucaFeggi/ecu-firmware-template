#include "hardware.hpp"

#include "backend.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include <stm32h7xx.h>

namespace target::hardware {
namespace {

using namespace drivers::stm32h7;

constexpr Pin pin(Port port, std::uint8_t number,
                  std::uint8_t alternate_function = 0U) noexcept {
  return {port, number, alternate_function};
}

constexpr std::array<CanFilter, 1U> can_standard_filters{{
    {CanFilterMatch::exact, 0x123U, 0U},
}};
constexpr std::array<CanFilter, 1U> can_fd_extended_filters{{
    {CanFilterMatch::exact, 0x18FF50E6U, 0U},
}};

DigitalInputStorage button_storage{
    .pin = pin(Port::c, 13U),
    .pull = Pull::down,
    .active_high = true,
};
DigitalOutputStorage led_storage{
    .pin = pin(Port::e, 3U),
    .pull = Pull::none,
    .output_type = OutputType::push_pull,
    .speed = Speed::low,
    .active_high = true,
    .initial_active = false,
};
DigitalOutputStorage phy_reset_storage{
    .pin = pin(Port::e, 7U),
    .pull = Pull::none,
    .output_type = OutputType::push_pull,
    .speed = Speed::low,
    .active_high = false,
    .initial_active = true,
};

AdcStorage adc_storage{
    .reduction = {drivers::AdcReduction::moving_mean, 16U},
    .full_scale = 4'095U,
};
const std::array<AdcScanChannel, 1U> adc_channels{{
    {pin(Port::c, 0U), 10U, 7U, &adc_storage},
}};
AdcScanStorage adc_scan_storage{
    .instance = AdcInstance::adc1,
    .channels = adc_channels.data(),
    .channel_count = adc_channels.size(),
    .oversampling = {1U, 0U},
    .clock_prescaler_encoding = 11U,
};

PwmStorage pwm_storage{
    .timer = TimerInstance::tim1,
    .channel = 1U,
    .pin = pin(Port::a, 8U, 1U),
    .timer_clock_hz = 120'000'000U,
    .frequency_hz = 1'000U,
    .active_high = true,
    .safe_duty = {0U},
};
SerialStorage serial_storage{
    .instance = SerialInstance::usart1,
    .transmit = pin(Port::a, 9U, 7U),
    .receive = pin(Port::a, 10U, 7U),
    .peripheral_clock_hz = 120'000'000U,
    .baud_rate = 115'200U,
    .now_us = monotonic_microseconds,
};
SpiDeviceStorage spi_storage{
    .instance = SpiInstance::spi1,
    .clock = pin(Port::b, 3U, 5U),
    .miso = pin(Port::b, 4U, 5U),
    .mosi = pin(Port::d, 7U, 5U),
    .chip_select = pin(Port::d, 6U),
    .peripheral_clock_hz = 40'000'000U,
    .maximum_clock_hz = 10'000'000U,
    .mode = 0U,
    .now_us = monotonic_microseconds,
};
I2cDeviceStorage i2c_storage{
    .instance = I2cInstance::i2c1,
    .clock = pin(Port::b, 8U, 4U),
    .data = pin(Port::b, 9U, 4U),
    .target_address = 0x42U,
    .timing_register = 0x40805E8AU,
    .now_us = monotonic_microseconds,
};
CanStorage can_storage{
    .instance = CanInstance::fdcan1,
    .receive_pin = pin(Port::d, 0U, 9U),
    .transmit_pin = pin(Port::d, 1U, 9U),
    .timing = {8U, 4U, 15U, 4U},
    .filters = {can_standard_filters.data(), can_standard_filters.size(),
                nullptr, 0U},
    .now_us = monotonic_microseconds,
};
CanFdStorage can_fd_storage{
    .instance = CanInstance::fdcan2,
    .receive_pin = pin(Port::b, 5U, 9U),
    .transmit_pin = pin(Port::b, 6U, 9U),
    .nominal_timing = {8U, 4U, 15U, 4U},
    .data_timing = {4U, 2U, 7U, 2U},
    .filters = {nullptr, 0U, can_fd_extended_filters.data(),
                can_fd_extended_filters.size()},
    .now_us = monotonic_microseconds,
};
BlockDeviceStorage block_storage{
    .instance = SdmmcInstance::sdmmc1,
    .clock = pin(Port::c, 12U, 12U),
    .command = pin(Port::d, 2U, 12U),
    .data = {pin(Port::c, 8U, 12U), pin(Port::c, 9U, 12U),
             pin(Port::c, 10U, 12U), pin(Port::c, 11U, 12U)},
    .kernel_clock_hz = 80'000'000U,
    .transfer_clock_hz = 24'000'000U,
    .now_us = monotonic_microseconds,
};
RtcStorage rtc_storage{};
NvMemoryStorage nv_storage{
    .address = 0x081E0000U,
    .size = 0x00020000U,
    .erase_parallelism_encoding = 2U,
    .now_us = monotonic_microseconds,
};
WatchdogStorage watchdog_storage{
    .lsi_frequency_hz = 32'000U,
    .now_us = monotonic_microseconds,
};

__attribute__((section(".eth_dma"), aligned(16384)))
EthernetMacStorage ethernet_storage{
    .rmii_pins = {pin(Port::a, 1U, 11U), pin(Port::a, 2U, 11U),
                  pin(Port::a, 7U, 11U), pin(Port::c, 1U, 11U),
                  pin(Port::c, 4U, 11U), pin(Port::c, 5U, 11U),
                  pin(Port::b, 11U, 11U), pin(Port::b, 12U, 11U),
                  pin(Port::b, 13U, 11U)},
    .mac_address = {{std::byte{0x02}, std::byte{0x00}, std::byte{0x00},
                     std::byte{0x00}, std::byte{0x07}, std::byte{0x43}}},
    .phy_address = 0U,
    .hclk_hz = 120'000'000U,
    .now_us = monotonic_microseconds,
    .speed_100_mbps = true,
    .full_duplex = true,
};
static_assert(sizeof(ethernet_storage) <= 16U * 1024U);

auto button_driver = bind(button_storage);
auto led_driver = bind(led_storage);
auto phy_reset_driver = bind(phy_reset_storage);
auto adc_driver = bind(adc_storage);
auto pwm_driver = bind(pwm_storage);
auto serial_driver = bind(serial_storage);
auto spi_driver = bind(spi_storage);
auto i2c_driver = bind(i2c_storage);
auto can_driver = bind(can_storage);
auto can_fd_driver = bind(can_fd_storage);
auto block_driver = bind(block_storage);
auto ethernet_driver = bind(ethernet_storage);
auto rtc_driver = bind(rtc_storage);
auto nv_driver = bind(nv_storage);
auto watchdog_driver = bind(watchdog_storage);

std::atomic<std::uint32_t> initialized_drivers{0U};

constexpr std::uint32_t bit(Driver driver) noexcept {
  return 1UL << static_cast<std::uint32_t>(driver);
}

template <typename Result>
void remember(Driver driver, const Result& result) noexcept {
  if (result) {
    initialized_drivers.fetch_or(bit(driver), std::memory_order_relaxed);
  }
}

} // namespace

bool initialize() noexcept {
  initialized_drivers.store(0U, std::memory_order_relaxed);
  const auto button = drivers::stm32h7::initialize(button_storage);
  remember(Driver::gpio_input, button);
  const auto led = drivers::stm32h7::initialize(led_storage);
  remember(Driver::gpio_output, led);
  static_cast<void>(drivers::stm32h7::initialize(phy_reset_storage));

  const auto serial_result = drivers::stm32h7::initialize(serial_storage);
  remember(Driver::serial, serial_result);
  const auto adc_result = drivers::stm32h7::initialize(adc_storage);
  if (adc_result) {
    const auto scan_result = drivers::stm32h7::initialize(adc_scan_storage);
    remember(Driver::adc, scan_result);
  }
  const auto pwm_result = drivers::stm32h7::initialize(pwm_storage);
  remember(Driver::pwm, pwm_result);
  const auto spi_result = drivers::stm32h7::initialize(spi_storage);
  remember(Driver::spi, spi_result);
  const auto i2c_result = drivers::stm32h7::initialize(i2c_storage);
  remember(Driver::i2c, i2c_result);
  const auto can_result = drivers::stm32h7::initialize(can_storage);
  remember(Driver::can, can_result);
  const auto can_fd_result = drivers::stm32h7::initialize(can_fd_storage);
  remember(Driver::can_fd, can_fd_result);
  const auto rtc_result = drivers::stm32h7::initialize(rtc_storage);
  remember(Driver::rtc, rtc_result);
  const auto nv_result = drivers::stm32h7::initialize(nv_storage);
  remember(Driver::nv_memory, nv_result);
  const auto watchdog_result = drivers::stm32h7::initialize(watchdog_storage);
  remember(Driver::watchdog, watchdog_result);

  if (phy_reset_storage.initialized) {
    const auto reset_started = monotonic_microseconds();
    while (monotonic_microseconds() - reset_started < 10'000U) {
    }
    phy_reset_driver.set_active(false);
    const auto reset_released = monotonic_microseconds();
    while (monotonic_microseconds() - reset_released < 50'000U) {
    }
  }
  const auto ethernet_result = drivers::stm32h7::initialize(ethernet_storage);
  remember(Driver::ethernet, ethernet_result);
  const auto block_result = drivers::stm32h7::initialize(block_storage);
  remember(Driver::block_device, block_result);

  if (ready(Driver::adc)) {
    NVIC_SetPriority(ADC_IRQn, 6U);
    NVIC_EnableIRQ(ADC_IRQn);
    const auto started = drivers::stm32h7::start(adc_scan_storage);
    if (!started) {
      initialized_drivers.fetch_and(~bit(Driver::adc),
                                    std::memory_order_relaxed);
    }
  }
  return ready(Driver::gpio_input) && ready(Driver::gpio_output) &&
         ready(Driver::serial) && ready(Driver::watchdog);
}

bool ready(Driver driver) noexcept {
  return (ready_mask() & bit(driver)) != 0U;
}

std::uint32_t ready_mask() noexcept {
  return initialized_drivers.load(std::memory_order_relaxed);
}

drivers::Adc& adc() noexcept { return adc_driver; }
drivers::BlockDevice& block_device() noexcept { return block_driver; }
drivers::Can& can() noexcept { return can_driver; }
drivers::CanFd& can_fd() noexcept { return can_fd_driver; }
drivers::EthernetMac& ethernet() noexcept { return ethernet_driver; }
drivers::DigitalInput& gpio_input() noexcept { return button_driver; }
drivers::DigitalOutput& gpio_output() noexcept { return led_driver; }
drivers::I2cDevice& i2c() noexcept { return i2c_driver; }
drivers::NvMemory& nv_memory() noexcept { return nv_driver; }
drivers::Pwm& pwm() noexcept { return pwm_driver; }
drivers::Rtc& rtc() noexcept { return rtc_driver; }
drivers::Serial& serial() noexcept { return serial_driver; }
drivers::SpiDevice& spi() noexcept { return spi_driver; }
drivers::Watchdog& watchdog() noexcept { return watchdog_driver; }

void handle_adc_irq() noexcept {
  drivers::stm32h7::handle_adc_interrupt(adc_scan_storage);
}
void handle_can_irq() noexcept {
  drivers::stm32h7::handle_can_receive_interrupt(can_storage);
}
void handle_can_fd_irq() noexcept {
  drivers::stm32h7::handle_can_fd_receive_interrupt(can_fd_storage);
}

bool connect_can_receive_event(void (*callback)(void*) noexcept,
                               void* context) noexcept {
  if (!ready(Driver::can) ||
      !drivers::stm32h7::connect_receive_event(can_storage, callback,
                                                context) ||
      !drivers::stm32h7::enable_receive_interrupt(can_storage)) {
    return false;
  }
  NVIC_SetPriority(FDCAN1_IT0_IRQn, 6U);
  NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
  return true;
}

bool connect_can_fd_receive_event(void (*callback)(void*) noexcept,
                                  void* context) noexcept {
  if (!ready(Driver::can_fd) ||
      !drivers::stm32h7::connect_receive_event(can_fd_storage, callback,
                                                context) ||
      !drivers::stm32h7::enable_receive_interrupt(can_fd_storage)) {
    return false;
  }
  NVIC_SetPriority(FDCAN2_IT0_IRQn, 6U);
  NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
  return true;
}

} // namespace target::hardware
