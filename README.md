# ECU firmware template

FreeRTOS-specific C++20 ECU application template with compile-time selected
driver backends and exact target configurations. The sample application follows
the SMU task style: static tasks and queues, direct FreeRTOS APIs, semantic
hardware objects, ISR-to-task notification, a startup event gate, and
supervisor-only watchdog refresh.

For a local host build with sibling driver repositories:

```sh
cmake --preset host-debug -DFREERTOS_KERNEL_DIR=/path/to/FreeRTOS-Kernel
cmake --build --preset host-debug
ctest --preset host-debug
```

Release embedded targets remain intentionally configuration-gated until their
schematic-derived pins, clocks, transceivers, safe states, and memory map are
populated under the exact `config/<target>/` directory.

## Implementation status

The interface, complete deterministic POSIX backend, POSIX target binding, and
SMU-style FreeRTOS sample application are implemented and host-tested. The
direct-register STM32F446xx, STM32H563xx, and STM32H723xx backends currently
implement GPIO, ADC, PWM, serial, SPI, classic CAN, watchdog, RTC, and internal
flash nonvolatile memory; they compile with the corresponding Arm target flags
and CMSIS device headers but still require target-specific smoke and HIL tests.

SDIO/SDMMC block-device support and the H563/H723 Ethernet MAC data paths are
not implemented yet. The ESP32-S3-WROOM-1-N16R8 ESP-IDF component implements
GPIO, ADC, PWM, serial, SPI, TWAI classic CAN, watchdog, flash partitions, and
SDMMC block access, but must be compiled and validated with the pinned ESP-IDF
6.1 SDK. Unsupported component selections and all embedded firmware targets
without reviewed schematic bindings fail during CMake configuration; no stub
implementation is linked.

The NXP i.MX 8M Plus Cortex-M7 backend implements direct-register GPIO, PWM,
UART, ECSPI, I2C, FlexCAN, WDOG and SNVS RTC support, plus the VCU v9 TLA2024
ADC and MCP2518FD classic-CAN protocols. It cross-compiles for Cortex-M7 with
the official `MIMX8ML8_cm7` device header. The
`verdin-imx8mp-quad-4gb-wb-it-vcu-v9-m7` firmware target remains deliberately
fail-closed until its A53/M7 resource partition, exact IOMUX/clock operations,
electrical safe states, interrupt routing and HIL acceptance are reviewed.
