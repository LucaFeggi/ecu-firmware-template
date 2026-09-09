# STM32H563VIT6 target inputs

This directory is reserved for one reviewed STM32H563VIT6 board mapping. The
current direct-register capability subset is cross-compiled, but a part number
is not a board definition. Before this target can link, commit:

- PCB/schematic revision and boot/debug interface;
- HSE/LSE sources, voltage scale and complete clock tree;
- TrustZone policy (the initial architecture requires a single non-secure image);
- application pins, safe GPIO levels and alternate functions;
- FDCAN instance, nominal timing, message-RAM/IRQ ownership and transceiver pins;
- ADC1/ADC2 channels, trigger/acquisition rate and shared hardware-oversampling
  settings, plus each sensor's independent software reduction;
- PWM, serial and SPI resource ownership;
- watchdog timeout validated against LSI tolerance;
- RTC backup-domain and retention assumptions;
- internal-flash NV region and erase/endurance strategy;
- SDMMC/SPI card wiring when `BlockDevice` is selected;
- RMII/MII mode, PHY model/address/reset, reference clock and DMA buffers when
  `EthernetMac` is selected;
- linker memory regions, stack budget, DMA placement, cache/MPU policy and IRQ
  priorities.

The firmware build remains fail-closed until those values and the required
startup, linker, FreeRTOS and binding files are reviewed. Passing the backend
cross-build is not a substitute for target smoke and HIL validation.
