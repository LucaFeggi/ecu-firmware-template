# ESP32-S3-WROOM-1-N16R8 target inputs

This target uses the official module ordering code for the 16 MB flash / 8 MB
Octal-PSRAM module. It is built as an ESP-IDF project rather than through the STM
startup/linker flow. Before enabling it, commit:

- PCB/schematic revision, strapping pins and USB/JTAG/UART programming path;
- the exact ESP-IDF release and toolchain lock (the backend declares its tested
  range independently);
- GPIO mapping and boot-safe output levels;
- ADC unit/channels, attenuation, calibration and continuous sampling/reduction;
- LEDC/MCPWM, UART and SPI resource ownership;
- TWAI pins, bitrate, transceiver enable/standby and alert/ISR policy;
- task-watchdog subscriptions and fault behavior;
- named flash partition used for `NvMemory`, with erase/endurance policy;
- SDMMC/SDSPI wiring and media detect/write-protect behavior when a block device
  is present;
- an explicit external battery-backed RTC when `Rtc` is requested;
- an explicit raw-frame external Ethernet controller/PHY path when
  `EthernetMac` is requested;
- reviewed `sdkconfig.defaults`, `partitions.csv`, static task/queue budgets and
  secure-boot/flash-encryption provisioning policy.

ESP-IDF may allocate inside pinned platform components, but project-owned driver
state and steady-state application resources remain bounded and static. This
configuration is not enabled merely because the module can boot an IDF example.
