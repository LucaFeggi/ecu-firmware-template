# STM32H723VGT6 target inputs

This directory is reserved for one reviewed STM32H723VGT6 board mapping. The
current direct-register capability subset is cross-compiled. Release
configuration still requires:

- authoritative PCB/schematic revision, boot straps and debug/programming path;
- oscillator sources, voltage scale, flash latency and complete clock tree;
- exact pins, alternate functions and reset/fault-safe output levels;
- selected FDCAN1/2/3 instance, timing, transceiver controls and IRQ priority;
- ADC instance/resolution, channel acquisition times, sample trigger and shared
  hardware oversampling, with software reduction per sensor;
- PWM, serial and SPI ownership and all DMA/IRQ mappings;
- watchdog timeout validated across LSI tolerance;
- RTC backup-domain power/clock retention;
- reserved flash NV region and endurance strategy;
- SDMMC/SPI block-media wiring;
- RMII/MII PHY model/address/reset/reference clock when Ethernet is selected;
- a reviewed Cortex-M7 MPU/cache policy and non-cacheable or explicitly
  maintained DMA descriptor/buffer regions;
- exact linker map, stack budget and FreeRTOS interrupt-priority contract.

Do not enable the root target by filling arbitrary example values. It becomes a
supported target only after compile, on-target smoke, bus, storage, Ethernet and
fault-injection/HIL acceptance appropriate to the selected capabilities.

## FDCAN binding

STM32H723VGT6 has three instances of the same FDCAN peripheral: FDCAN1,
FDCAN2 and FDCAN3. They share the FDCAN message RAM. Bind each physical CAN bus
to exactly one instance and exactly one driver type:

- `drivers::stm32h7::CanStorage` and `drivers::Can` for a classical CAN bus;
- `drivers::stm32h7::CanFdStorage` and `drivers::CanFd` for a CAN FD bus.

Do not bind both types to the same FDCAN instance. CAN FD does not need an
additional `Can` object.

The board binding must provide the selected instance, RX/TX pins and alternate
function, FDCAN kernel clock, nominal timing, and—when using CAN FD—data-phase
timing. `bit_rate_switch` is per frame; it uses the one configured data-phase
timing and never selects a rate itself.

Define only the messages used by the firmware as static `CanFilter` arrays,
then pass them through `CanAcceptanceFilters` in the storage object. Separate
arrays configure standard and extended identifiers. Each entry is exact, mask,
or inclusive range matching. Empty lists accept no data frames; remote frames
and unmatched data frames are rejected by FDCAN before the software queue.

The binding must dispatch the selected FDCAN receive IRQ to
`handle_can_receive_interrupt(storage)` or
`handle_can_fd_receive_interrupt(storage)`, respectively, after configuring
the NVIC priority within the FreeRTOS interrupt-priority contract.
