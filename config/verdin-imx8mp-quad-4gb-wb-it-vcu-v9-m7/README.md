# Verdin iMX8M Plus Quad 4GB WB IT / VCU v9 M7 target inputs

This configuration is the FreeRTOS image for the Cortex-M7 core of the Toradex
Verdin iMX8M Plus Quad 4GB WB IT module on the VCU v9 carrier/shield. Linux on
the Cortex-A53 cores is a peer owner of the SoC, not a driver backend.

## Hardware composition currently identified

- FlexCAN1 and FlexCAN2 connect through MAX14878 transceivers;
- the third CAN controller is an external MCP2518FD on ECSPI1, also through a
  MAX14878;
- all three transceiver paths are limited to classic CAN at up to 1 Mbit/s by
  the selected physical layer, so the public `Can` contract remains 8-byte
  classic CAN;
- ECSPI1 uses the active-design Verdin signals on connector pins 196, 198, 200,
  and 202; the MCP2518FD interrupt signals use GPIO1_IO0, GPIO1_IO1, and
  GPIO1_IO5 on pins 206, 208, and 210;
- ADC0..ADC3 use the module/carrier TLA2024 at 7-bit I2C address `0x49`;
- ADC4..ADC7 use the second TLA2024 at address `0x48`;
- both converters share I2C1, which also carries module PMIC, RTC, and EEPROM
  traffic;
- the M7 image is expected to own the three PWM outputs, UART1 for external GPS,
  UART4 for M7 diagnostics, and its safety watchdog;
- Ethernet, microSD, HDMI, USB, and UART3 Linux diagnostics are initially owned
  by Linux and are not requested from the M7 backend.

The authoritative schematic/project revision and connector table must be
committed beside the eventual binding. Net names alone are not sufficient
acceptance evidence.

## Release-blocking decisions

Before adding `CMakeLists.txt`, `driver_bindings.cpp`, startup, linker, and
FreeRTOS configuration files, review and commit:

- exclusive A53/M7 ownership of every CCM clock gate, reset, IOMUX pad,
  peripheral, interrupt, and DMA channel used by M7;
- a specific I2C1 owner and arbitration model. The initial safe choice is one
  owner only; Linux and M7 must not both program the controller;
- Linux remoteproc/resource-table and device-tree changes that reserve the M7
  firmware memory and disable Linux ownership of M7-assigned peripherals;
- M7 boot/recovery, watchdog, fault, and A53 communication behavior;
- exact clock roots and frequencies supplied to FlexCAN, UART, ECSPI, I2C, PWM,
  GPT/FreeRTOS tick, and WDOG;
- complete IOMUX and pad-control values, interrupt priorities, and electrically
  safe reset states;
- MCP2518FD oscillator, SPI ceiling, classic-CAN timing, FIFO/filter allocation,
  interrupt behavior, and recovery from SPI/CAN faults;
- TLA2024 input range, channel mapping, data rate, conversion-ready policy,
  source impedance, scaling, calibration, per-sensor software reduction, and
  freshness deadlines;
- SNVS RTC ownership and retained-power validity, or explicit omission of
  `Rtc` from the M7 component list;
- Cortex-M7 TCM/OCRAM/linker layout, MPU/cache policy, shared-memory placement,
  stack budgets, and image-loading address;
- the exact MCUXpresso SDK/device-header, CMSIS, compiler, FreeRTOS, Linux/BSP,
  and device-tree commits.

The target stays fail-closed until these values exist. Do not copy clock, IOMUX,
linker, or resource-table values from an EVK example without validating them
against the Verdin module, carrier, and running Linux BSP.
