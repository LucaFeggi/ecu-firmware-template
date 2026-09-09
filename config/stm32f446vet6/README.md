# STM32F446VET6 target inputs

This directory is reserved for one reviewed STM32F446VET6 board mapping. The
current direct-register capability subset is cross-compiled, but this firmware
target is intentionally not linkable until the following schematic-derived
inputs are committed:

- PCB and schematic revision;
- HSE/LSE sources, tolerances and clock tree;
- every application pin and alternate function;
- CAN controller, bitrate, transceiver, standby/enable polarity and IRQ priority;
- ADC channels, acquisition rate, trigger timer/DMA mapping and sensor reduction;
- PWM timer channels, polarities and electrically safe reset/fault levels;
- diagnostic serial mapping;
- watchdog period derived from the measured LSI range;
- RTC backup-power and clock-retention contract;
- reserved flash sector for nonvolatile storage, including endurance policy;
- SDIO/SPI media wiring if a block device is enabled;
- flash/RAM/linker map, stack budget and interrupt-priority policy;
- programming/debug interface and fatal-fault indication.

Do not copy a mapping from another ECU merely because it uses the same MCU.
Populate the architecture-required files from the authoritative board design,
then remove the root CMake release gate only after compile, smoke and HIL tests.
