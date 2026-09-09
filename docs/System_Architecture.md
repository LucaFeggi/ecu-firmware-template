# ECU Firmware System Architecture

## Status

This document defines the target architecture for the ECU firmware and its
portable driver library. The existing driver repositories may be rewritten; they
are not compatibility constraints for this design.

The architecture covers these targets. A target is release-supported only when
its capability row is not marked planned/gated and its acceptance evidence is
complete:

- STM32F446VET6
- STM32H563VIT6
- STM32H723VGT6
- ESP32-S3-WROOM-1-N16R8 module, marked `MON16R8`, with 16 MB flash and
  8 MB PSRAM
- NXP i.MX 8M Plus `MIMX8ML8` Cortex-M7 on the Verdin iMX8M Plus Quad
  4GB WB IT / VCU v9 platform; Linux remains on the Cortex-A53 cores
- POSIX, for development and host-side tests

## Design goals

The design must provide:

- one small, stable C++ interface for each peripheral;
- one selected implementation at build time;
- unchanged application and FreeRTOS task logic across targets;
- no dynamic allocation in embedded driver code;
- no dependency on STM32 HAL/LL, ESP-IDF or FreeRTOS in the public driver
  interfaces;
- explicit failure when a target configuration requests an unsupported feature;
- target-specific pin, clock and peripheral mappings outside application logic;
- a shallow source tree that remains understandable without knowing the target's
  peripheral taxonomy.

### Meaning of embedded-safe in this document

All code under `app/`, embedded `config/` directories and embedded driver
backends is firmware code. Examples in this document are normative about safety
properties even when normal includes and private construction details are
omitted. They must use:

- static or caller-owned storage with compile-time capacities;
- no C++ exceptions, RTTI, `new`, `delete`, heap-owning containers or
  heap-owning callbacks in project code;
- bounded loops, queues, transfers, retries and timeouts;
- checked arithmetic for sizes, frequencies, deadlines and register fields;
- explicit handling of every fallible result before consuming its value;
- safe actuator states before initialization and on every fatal path;
- documented single-writer/concurrency ownership and bounded ISR work;
- fixed-width serialized formats with explicit byte order, never raw C++ object
  layout;
- measured stack, RAM, flash, execution-time and interrupt-latency budgets.

The POSIX target and host tests do not weaken these contracts. They exercise the
same bounded interfaces and application logic even though the host itself has a
heap and is not a real-time platform. ESP-IDF may allocate inside its pinned
platform components, but project-owned ESP32-S3 drivers, tasks, queues and
steady-state paths follow the same rules unless a documented platform service
requires a separately budgeted exception.

The driver library deliberately has only two architectural layers:

1. public driver interfaces;
2. the selected target implementation.

Private helper files inside an implementation do not constitute another public
layer.

## Repository layout

The firmware repository should have the following high-level structure:

```text
ecu-firmware-template/
├── CMakeLists.txt
├── CMakePresets.json
├── app/
├── cmake/
│   └── toolchains/
├── config/
│   ├── stm32f446vet6/
│   ├── stm32h563vit6/
│   ├── stm32h723vgt6/
│   ├── esp32-s3-wroom-1-n16r8/
│   ├── verdin-imx8mp-quad-4gb-wb-it-vcu-v9-m7/
│   └── posix/
├── lib/
│   ├── drivers/                 # Git submodule: ecu-drivers
│   └── freertos/                # upstream kernel, when managed by this repository
├── tests/
└── docs/
```

Each single-core configuration directory is the normalized lowercase full
identifier of the selected MCU or module. `posix` is the corresponding host
target. The heterogeneous Verdin directory additionally names the carrier
revision and M7 image because that cross-core resource partition changes the
binary. Board revision, oscillator, pinout and peripheral-routing information
lives inside that directory without adding an ECU role prefix.

`lib/` is the firmware repository's single container for code linked as a
library, whether project-owned or upstream and whether checked out directly or
through a Git submodule. Ownership is recorded by the submodule URL, license and
component documentation rather than by a second top-level directory. Therefore
the project-owned driver library lives at `lib/drivers`; a repository-managed
FreeRTOS kernel may live at `lib/freertos`.

Do not create an empty top-level `third_party/` directory. It becomes useful only
if the firmware repository itself must vendor non-library upstream material that
does not naturally belong to a component. There is no such requirement in the
initial architecture. Test frameworks can be obtained by the host test build or
placed below `lib/` if they must be pinned as source checkouts.

`lib/drivers` is the checkout of the `ecu-drivers` superproject. Its public
interfaces live directly in that repository, while independently versioned
backends are nested Git submodules. This makes adding a new MCU family a localized
operation and allows each backend to evolve at its own rate.

### Sample application layout

The template contains one small but complete FreeRTOS application. It is a
reference for task lifecycle, ownership, fault handling and driver use; it is not
an additional portability layer and it does not attempt to model a complete ECU.

```text
app/
├── CMakeLists.txt
├── app.cpp
├── app.hpp
├── app_context.hpp
├── freertos_task.hpp
├── hardware.hpp
├── system_fault.hpp
├── timing.hpp
└── tasks/
    ├── sensing/
    │   ├── sensing_task.cpp
    │   └── sensing_task.hpp
    ├── can_rx/
    │   ├── can_rx_task.cpp
    │   └── can_rx_task.hpp
    ├── can_tx/
    │   ├── can_tx_queue.cpp
    │   ├── can_tx_queue.hpp
    │   ├── can_tx_task.cpp
    │   └── can_tx_task.hpp
    ├── led/
    │   ├── led_task.cpp
    │   └── led_task.hpp
    └── supervisor/
        ├── supervisor_task.cpp
        ├── supervisor_task.hpp
        └── task_heartbeat.hpp
```

The initial sample deliberately has no separate sensor layer. Its small
amount of conversion and plausibility logic remains local to the sensing task.
If that logic later becomes independently reusable or testable, it may be
extracted based on the real domain model rather than pre-creating an abstraction.

The three application-support headers have narrow purposes:

- `hardware.hpp` is the semantic hardware map seen by tasks. It declares roles
  such as `vehicle_bus`, `accelerator_primary` and `cooling_fan`, plus the few
  application interrupt connections that are genuinely required. Exactly one
  `config/<target>/driver_bindings.cpp` defines those symbols using physical pins
  and peripherals. It contains no register IDs, backend types or ownership of
  application state.
- `app_context.hpp` defines the one statically allocated `AppContext` shared by
  the task modules. It owns only bounded inter-task state: startup-gate storage,
  fixed-capacity queue handles/storage where centrally owned, coherent published
  snapshots, health/fault state and heartbeat bookkeeping. It does not own
  drivers, allocate memory, initialize hardware or become a general service
  locator.
- `freertos_task.hpp` contains one checked static-task creation compatibility
  helper. It accepts a real `StackType_t` array, passes its exact capacity to
  `xTaskCreateStatic()`, handles the upstream-FreeRTOS versus ESP-IDF stack-unit
  difference and returns the native `TaskHandle_t`. It does not wrap scheduling,
  delays, queues or notifications and is not an RTOS abstraction.

`timing.hpp` contains checked application frequency/period-to-tick conversions.
There is no `app/targets/` directory because physical variation is already owned
by `config/`. A future behavioral product variant is a different concern and
must not be hidden inside the MCU binding.

### Target configuration contract

Every embedded STM32 configuration contains the following files:

```text
config/<exact-stm32-part>/
├── CMakeLists.txt
├── target_config.hpp
├── driver_bindings.cpp
├── system.cpp
├── startup.S
├── linker.ld
├── FreeRTOSConfig.h
└── README.md
```

- `CMakeLists.txt` declares the exact part, CPU flags, selected backend, required
  driver components, startup source and linker script. It is the only entry point
  included by the root build for that target.
- `target_config.hpp` contains compile-time oscillator, clock, voltage-scale,
  interrupt-priority and memory-region constants. It contains no application
  state.
- `driver_bindings.cpp` owns pins, alternate functions, peripheral instances,
  DMA requests, external-device wiring and semantic hardware objects.
- `system.cpp` implements early clock, flash, cache, MPU and system-state setup.
- `startup.S` owns the vector table and reset path, initializes data/BSS, invokes
  C++ static initialization and enters `main`. A reviewed CMSIS template may be
  adapted, but the selected file is pinned by the target.
- `linker.ld` declares the exact flash/RAM map, stack, heap policy, DMA regions,
  retained/no-init data and exported boundary symbols.
- `FreeRTOSConfig.h` contains the target's reviewed kernel configuration.
- `README.md` records hardware assumptions that the compiler cannot prove.

The ESP32-S3 configuration follows ESP-IDF ownership instead of duplicating its
bootloader and linker system:

```text
config/esp32-s3-wroom-1-n16r8/
├── CMakeLists.txt
├── target_config.hpp
├── driver_bindings.cpp
├── sdkconfig.defaults
├── partitions.csv
└── README.md
```

The heterogeneous i.MX 8M Plus target must additionally define its relationship
with the Linux/A53 image:

```text
config/verdin-imx8mp-quad-4gb-wb-it-vcu-v9-m7/
├── CMakeLists.txt
├── target_config.hpp
├── driver_bindings.cpp
├── system.cpp
├── startup.S
├── linker.ld
├── FreeRTOSConfig.h
├── resource_table.c
└── README.md
```

The configuration pins the Linux BSP/device-tree and remoteproc contract as well
as the M7 image. Exactly one core owns each clock gate, reset, IOMUX pad,
peripheral, IRQ and DMA channel. Shared memory has a fixed protocol, address,
cache policy and lifecycle. Linux must not bind a device-tree driver to a
peripheral assigned to M7. In particular, I2C1 is not enabled for M7 until the
shared PMIC/RTC/EEPROM bus has one reviewed owner.

The POSIX configuration contains `CMakeLists.txt`, `target_config.hpp`,
`driver_bindings.cpp`, `FreeRTOSConfig.h` and `README.md`; it runs the same sample
tasks through the pinned FreeRTOS POSIX port. Driver unit and contract tests do
not require that port.

Every target `README.md` must state:

- exact device/module and package;
- supported PCB revision and the authoritative schematic revision;
- supply and oscillator assumptions;
- debug, boot and programming interface;
- external CAN transceiver, Ethernet PHY, RTC, storage and other required parts;
- safe reset state and active polarity of every actuator output;
- pins and peripherals intentionally unavailable because of conflicts;
- option-byte, TrustZone, flash-encryption or secure-boot assumptions;
- validated clock frequencies and a memory-budget summary.

A configuration directory represents one authoritative hardware and ownership
mapping. If two incompatible boards use the same MCU, or two i.MX images use a
different A53/M7 resource split, they must not be selected by preprocessor
conditionals inside one binding. They need distinct configuration identifiers or
separate product firmware repositories.

### Driver repository

```text
drivers/
├── .gitmodules
├── CMakeLists.txt
├── include/
│   ├── error.hpp
│   ├── gpio.hpp
│   ├── adc.hpp
│   ├── pwm.hpp
│   ├── serial.hpp
│   ├── spi.hpp
│   ├── i2c.hpp
│   ├── can.hpp
│   ├── watchdog.hpp
│   ├── rtc.hpp
│   ├── nv_memory.hpp
│   ├── block_device.hpp
│   └── ethernet_mac.hpp
├── backends/
│   ├── posix/                   # submodule: ecu-drivers-posix
│   ├── esp32s3/                 # submodule: ecu-drivers-esp32s3
│   ├── stm32f4/                 # submodule: ecu-drivers-stm32f4
│   │   ├── CMakeLists.txt
│   │   ├── backend/
│   │   │   ├── backend.hpp      # private target binding/storage API
│   │   │   ├── common.hpp       # private shared helpers, only when useful
│   │   │   ├── gpio.cpp
│   │   │   ├── adc.cpp
│   │   │   ├── pwm.cpp
│   │   │   ├── serial.cpp
│   │   │   ├── spi.cpp
│   │   │   ├── can.cpp
│   │   │   ├── watchdog.cpp
│   │   │   ├── rtc.cpp
│   │   │   ├── nv_memory.cpp
│   │   │   └── detail/          # optional complex private helpers
│   │   └── third_party/         # family-only pinned dependencies
│   ├── stm32h5/                 # submodule: ecu-drivers-stm32h5
│   ├── stm32h7/                 # submodule: ecu-drivers-stm32h7
│   └── imx8mp-m7/               # submodule: ecu-drivers-imx8mp-m7
└── tests/
    ├── unit/
    ├── contract/
    └── compile/
```

`backends/` is preferred to a top-level `src/` or `implementations/` directory in
the interface repository. It is short and communicates that the children are
interchangeable providers of the same interfaces. Inside every implementation
repository, all project-owned implementation files live under one `backend/`
directory. The implementation repository has no public `include/` directory:
the only public driver headers are those in `ecu-drivers/include/`.

`backend/backend.hpp` contains the selected target's private configuration and
static-storage types. Its include path is supplied only to the target binding
translation unit and the backend target itself; it never propagates to
application targets. Each implemented public capability has its own clearly
named `.cpp` file. `common.hpp` and `detail/` are private code-sharing mechanisms
and are present only when several driver files genuinely share low-level
helpers. Unsupported capabilities have no source file and are rejected during
CMake configuration.

The expanded `stm32f4` entry above demonstrates the consistent source-file
naming, not a requirement to create one stub for every public header. An
unsupported component source is absent and CMake rejects it. In particular,
`ethernet_mac.cpp` exists for F4 only when that backend intentionally supports a
declared external raw-frame Ethernet controller; it must not pretend the F446 has
an on-chip MAC.

Repository names use kebab-case. Checkout paths use short context-aware names, so
the repository `ecu-drivers-stm32h5` is checked out at `backends/stm32h5`, not at
`backends/stm32h5-drivers`. Source files and CMake identifiers may use snake_case.

The `ecu` prefix identifies the shared application domain, not ownership by one
specific control unit. `ecu-drivers` is consumed by SMU, PCU, VCU and other ECU
firmware repositories; it does not lead to separate `smu-drivers` or
`pcu-drivers` repositories. The alternatives `embedded-drivers` and
`mcu-drivers` are deliberately avoided because “Embedded” and “MCU” already name
specific Driverless hardware/control-unit concepts in this organization.

The naming layers are intentionally different and must not be collapsed into one
physical name. For the H5 vehicle-bus example:

```text
Repository:       ecu-drivers-stm32h5
Checkout path:    lib/drivers/backends/stm32h5
CMake target:     ecu_drivers_stm32h5
Public C++:       namespace drivers
Firmware role:    hardware::vehicle_bus
Physical binding: FDCAN1, pins, IRQ, message RAM
```

Repository names identify independently versioned projects. Checkout paths are
short because their parent already supplies context. CMake backend targets use
snake_case and identify what is compiled. Public C++ names express portable
capabilities, firmware roles express ECU meaning, and only the target binding
names physical hardware.

The remaining backend CMake targets follow the same rule:

```text
ecu_drivers_posix
ecu_drivers_esp32s3
ecu_drivers_stm32f4
ecu_drivers_stm32h5
ecu_drivers_stm32h7
ecu_drivers_imx8mp_m7
```

The `ecu-drivers` superproject exposes the selected composition to firmware as
`ecu_drivers`. Application CMake links that target and does not select or link a
family target directly.

Use `posix` only if the backend is genuinely POSIX-portable. If it depends on
Linux-only facilities such as SocketCAN, its repository and checkout directory
should be named `ecu-drivers-linux` and `backends/linux` instead.

### Submodule and interface compatibility

The backend-submodule model is reasonable for this project, but it must not rely
on the assumption that an interface will literally never change. Corrections,
new capabilities and stronger contracts will occasionally require coordinated
updates.

The `ecu-drivers` superproject is the compatibility manifest:

- it pins one tested commit of every backend submodule;
- CI builds the contract/compile tests for every pinned backend;
- releases of `ecu-drivers` identify a known-compatible set of commits.

Adding a target means creating its backend repository, adding it below
`backends/`, registering it with CMake, and adding a target configuration under
`config/`. A fresh checkout is initialized with recursive submodules. Only the
selected backend is compiled and linked.

### Backend-owned third-party dependencies

A dependency belongs to the closest component that requires and versions it.
Family-specific STM packages therefore live inside the corresponding backend
repository, not in the public interface repository and not in the firmware
application:

```text
ecu-drivers-stm32h5/
├── CMakeLists.txt
├── backend/
│   ├── backend.hpp
│   ├── common.hpp
│   ├── adc.cpp
│   ├── can.cpp
│   └── ...
└── third_party/
    ├── cmsis-core/              # pinned external dependency
    └── cmsis-device-h5/         # pinned family device package
```

The F4 and H7 backends follow the same rule with their own device packages. This
makes a backend repository self-contained and pins the CMSIS versions against
which its register code was tested. These nested dependencies are initialized by
`git submodule update --init --recursive` from the firmware repository.

Only CMSIS Core and the relevant CMSIS Device package are required for the
direct-register approach. Do not add a complete STM32Cube/HAL repository unless a
separate, concrete dependency requires it. CMSIS and vendor include directories
remain private to the backend CMake target.

If a toolchain later supplies a canonical CMSIS package, a backend may consume
that package instead of its local checkout, but one build must have exactly one
authoritative CMSIS version. It must not mix device headers from one release with
core headers selected accidentally from another include path.

The ESP32-S3 backend has a different dependency model. It is built as an ESP-IDF
component and declares the ESP-IDF versions it supports. The ESP32-S3 target
configuration pins the toolchain and ESP-IDF version and supplies reviewed
`sdkconfig` defaults. ESP-IDF is a platform SDK and build environment, so it is
not copied into the public interface repository; all of its headers and link
dependencies remain private to `ecu-drivers-esp32s3` and the ESP32-S3 firmware
target.

The i.MX 8M Plus M7 backend owns its pinned NXP dependencies in the same local
way as the STM32 backends. It consumes CMSIS Core and the official
`MIMX8ML8_cm7` device/feature/system headers from a pinned MCUXpresso SDK
release. It may adapt the matching startup and linker files as reviewed target
inputs. It does not link MCUXpresso `fsl_*` peripheral drivers: GPIO, PWM,
UART, ECSPI, I2C, FlexCAN, WDOG and SNVS behavior is project register code.
NXP headers and any startup reference remain private to
`ecu-drivers-imx8mp-m7` and the selected M7 configuration.

Public headers live directly in `include/`, keeping the repository layout flat
and allowing includes such as:

```cpp
#include <adc.hpp>
#include <can.hpp>
```

The flat include layout is appropriate for this closed firmware ecosystem and
its deliberately small interface set. Because flat layouts are more susceptible
to filename collisions, public header names must remain specific and consumers
must receive only the selected library's include path.
There is no `ru` namespace. Public C++ types still belong to
`namespace drivers`, while application and target-configuration code use their
own namespaces.

## Dependency direction

```text
Application and FreeRTOS tasks
             |
             v
Configuration-owned semantic bindings
             |
             v
    Public drivers interfaces
             |
             v
 One selected implementation
             |
             v
 Private platform support
  (CMSIS device or ESP-IDF)
             |
             v
       MCU or host platform

Target clock/pin/peripheral bindings -----> selected implementation
```

The application depends only on public interfaces and semantic hardware roles.
The selected target configuration is the composition root: it selects physical
peripherals, configures them, and gives initialized driver references to the
application. Configuration mapping is wiring, not a third driver abstraction
layer.

Dependencies must never point in the other direction. In particular:

- public driver headers do not include target-configuration headers;
- public driver headers do not include STM32 HAL, CMSIS device or ESP-IDF
  headers;
- driver implementations do not include application or FreeRTOS task headers;
- application logic does not include anything from `backends/`.

## Public interface design

### One interface per capability

There should not be a universal `Driver` base class. ADC, CAN and GPIO have
different natural lifecycles and forcing identical `init/start/stop/read/write`
methods makes the API less precise.

Each public header defines only:

- portable value types;
- configuration that has the same meaning on every target;
- the operations required by application logic;
- error and result types;
- explicit concurrency and ISR-safety contracts.

Public interfaces must not expose:

- `ADC_HandleTypeDef`, `FDCAN_HandleTypeDef`, or any other HAL type;
- STM32 register structure types;
- DMA stream/channel types from one MCU family;
- pin alternate-function numbers;
- FreeRTOS handles, queues, semaphores, or tick types;
- family names such as bxCAN or FDCAN when the application only needs CAN.

Prefer fixed-size values, `std::span`, `std::array`, `std::chrono` durations, and a
small `drivers::Result<T>`/`drivers::Error` abstraction. Avoid exceptions, RTTI,
heap allocation, and owning callbacks such as `std::function` in embedded paths.

### One interface header may contain several related types

In this document, a driver *interface* means the complete contract for one
hardware capability, not necessarily one C++ class per file. A header can contain:

- value types, such as `CanFrame`, `DutyCycle` or `AdcSample`;
- enums/configuration types, such as `CanErrorState` or `AdcReduction`;
- one or more non-owning capability handles when the hardware has materially
  different safe usage modes.

`gpio.hpp` has `DigitalInput` and `DigitalOutput` because making direction part of
the type prevents writing to an input or reading an output through the wrong API.
They are two typed views of GPIO capability, not two backend implementations.
`adc.hpp` has `Adc`, a non-destructive view of one channel of an already running
continuous scan engine. `latest()` never starts or waits for a conversion.

The selected backend still implements the entire header for the selected target.
Having several types does not cause several backends to be linked and does not
mean every MCU has multiple physical peripherals. If two types stop sharing
vocabulary, implementation resources and review context, they should be split
into separately named capability headers. The rule is cohesion and misuse
prevention, not “one class per file.”

### Compile-time backend selection, not runtime polymorphism

The public declarations are the same for every target. The chosen backend
provides their definitions. Only one backend is linked into an embedded image.
This avoids virtual dispatch and prevents unused MCU implementations from being
compiled into the firmware.

A public driver object may contain a small stable handle, such as a slot index or
`std::uintptr_t`. Backend state and interrupt bookkeeping remain in static storage
owned by the selected implementation. This makes public object layout independent
of HAL structures and requires no dynamic allocation.

Do not design a generic traits or peripheral-IP framework in advance. Separate H5
and H7 source files are acceptable even when initially similar. Extract a shared,
private helper only after real duplication is demonstrated and the hardware
semantics are actually identical.

### Public object representation and lifecycle

Every stateful driver object uses one implementation-independent machine-word
handle:

```cpp
namespace drivers {

class Can final {
public:
    Can() = delete;
    Can(const Can&) = delete;
    Can& operator=(const Can&) = delete;
    Can(Can&&) = delete;
    Can& operator=(Can&&) = delete;
    ~Can() = default;

    // Public operations omitted.
    explicit constexpr Can(std::uintptr_t handle) noexcept : handle_{handle} {}

private:
    std::uintptr_t handle_;
};

} // namespace drivers
```

The handle is normally a nonzero word; zero is reserved for backend-internal
invalid-state detection. A selected backend may interpret the word as a pointer
to aligned static state or as a slot token. No public header contains the backend
state type. Target-binding code constructs the driver explicitly from that handle;
application code should receive only already configured driver objects.

Driver objects are non-owning, non-copyable and non-movable. Application code
passes them by reference. Their destructors never touch hardware. Backend state,
DMA buffers, queues and interrupt bookkeeping have static storage and outlive the
firmware. The initial API does not support hot backend replacement or normal
peripheral destruction.

Binding and initialization are deliberately separate:

1. static target composition connects a public object to static backend state;
2. constructors perform no register access, waiting, allocation or failure-prone
   work;
3. `hardware::initialize()` validates and initializes each physical resource once;
4. fallible methods return `not_initialized` if called before successful
   initialization; infallible methods treat the same misuse as a contract fault;
5. tasks receive references only after the initialization gate succeeds.

Initialization must be idempotent for an already initialized object with the same
configuration. A conflicting second initialization is an error. When an
initialization step fails, the backend disables its interrupts and DMA requests,
returns outputs to their documented safe state where possible, and returns the
most specific `Error`; it must not leave a partially active peripheral and report
success.

The public interface retains compile-time layout assertions. Binary compatibility
between arbitrary repository revisions is not a goal; a pinned `ecu-drivers`
superproject revision is the compatibility unit.

### Interface scope and optional features

The repository may contain the union of interfaces supported by its targets, but
each firmware image builds only the components it requests and its selected
device supports.

An unsupported peripheral must not be represented by a fake implementation that
always returns `unsupported`. The target configuration does not expose such a
device, and the build should fail during CMake configuration if the firmware
declares it as a required component.

`Can` is a classic-CAN contract with payloads up to 8 bytes. STM32F4 implements
it through bxCAN, STM32H5 and STM32H7 implement it through FDCAN operating in
classic mode, and ESP32-S3 implements it through its TWAI controller. The i.MX
8M Plus M7 backend maps the same contract to FlexCAN1, FlexCAN2, or the board's
MCP2518FD provider configured for classic frames. `CanFd` is a separate optional
contract for CAN FD data frames and bit-rate switching; it is selected only for
targets with a concrete `can_fd` implementation.

The same rule applies to advanced ADC APIs that expose DMA buffers or callback
pipelines, timer capture, low-power modes, and other capabilities that are not
genuinely common. The initial `Adc` contract exposes only a stable latest
sample from a configuration-owned continuous acquisition.

Interfaces should be added when firmware or test code needs them. Creating an
interface for every peripheral listed in every reference manual would produce a
large but unvalidated API.

### Initial public interface set

The initial interface repository contains only the capabilities required by the
firmware design:

| Header | Responsibility |
| --- | --- |
| `error.hpp` | Common `Error` and `Result<T>` definitions; this is support infrastructure, not a driver |
| `gpio.hpp` | Semantic digital inputs and outputs |
| `adc.hpp` | Analog sensor acquisition |
| `pwm.hpp` | Duty-cycle and frequency control for actuators |
| `serial.hpp` | Portable byte stream for diagnostics, logging or a console |
| `spi.hpp` | Transactions with external SPI sensors, memories and peripherals |
| `i2c.hpp` | Transactions with one statically addressed 7-bit I2C target |
| `can.hpp` | Classic CAN frame transmission, reception and filtering |
| `can_fd.hpp` | CAN FD data-frame transmission, reception and bit-rate switching |
| `watchdog.hpp` | Independent watchdog configuration and refresh |
| `rtc.hpp` | Calendar time retained through resets and backup power |
| `nv_memory.hpp` | Small byte-addressed persistent calibration, configuration and diagnostic data |
| `block_device.hpp` | Fixed-size block access for SD cards and block-oriented flash |
| `ethernet_mac.hpp` | Ethernet frame transfer between a network-stack adapter and the target's selected MAC |

`NvMemory` and `BlockDevice` are intentionally different contracts. `NvMemory`
is suitable for small persistent values and EEPROM emulation. `BlockDevice` is
sector-oriented bulk storage suitable for a filesystem. SDIO, SDMMC, QSPI or
OCTOSPI are backend implementation details when they only serve a block device.

### Capability naming rationale

Public filenames use `snake_case`; their principal C++ types use `PascalCase`.
Names describe the portable behavior seen by the caller rather than the bus,
vendor peripheral or current implementation:

- `ethernet_mac.hpp` / `EthernetMac` names the Ethernet media-access-control
  layer precisely. `ethernet.hpp` would be ambiguous about whether it includes a
  PHY, DHCP, IP, TCP or sockets, while `network.hpp` would be broader still. The
  interface moves link-layer Ethernet frames; therefore `EthernetMac` is the
  narrow correct name.
- `block_device.hpp` / `BlockDevice` is the established name for storage accessed
  as numbered, fixed-size blocks. It can represent an SD card, eMMC or suitable
  flash without exposing SDMMC/SPI/QSPI. `sd_card.hpp` would bind the application
  to one medium, and `storage.hpp` would not state whether access is byte-,
  record-, file- or block-oriented.
- `nv_memory.hpp` / `NvMemory` means nonvolatile, byte-addressed raw memory for
  small calibration/configuration records. It does not promise a specific
  technology. `eeprom.hpp` would be wrong for flash emulation, and `flash.hpp`
  would expose an implementation detail. `nonvolatile_memory.hpp` is also
  technically correct, but `NV memory` is standard embedded vocabulary and the
  shorter name is consistent with accepted domain abbreviations such as ADC,
  PWM, CAN and RTC.

These names deliberately describe three different abstraction levels: raw
Ethernet frames, numbered storage blocks and byte-addressed persistent memory.
Combining them under a generic `Device` or `Storage` name would hide important
alignment, lifetime and protocol contracts.

A generic `timer.hpp`, DMA, RCC, NVIC, cache control, SDMMC and USB are not part
of this initial public set. They remain private mechanisms or future capabilities
until application requirements justify a stable interface.

## First-version public interface contracts

The following declarations define the intended first-version contracts. They are
API sketches: normal standard-library includes, documentation comments and the
private construction mechanism are omitted for readability. Public application
code cannot construct a driver from a register address or vendor peripheral ID.
The selected target configuration creates each object through a backend-private
binding function.

All driver objects are concrete, non-virtual and non-owning. Each contains only a
small implementation-independent handle; the selected backend owns static state.
Objects are not dynamically allocated, and only one backend supplies the method
definitions in a firmware image. Configuration and initialization finish before
project tasks or application interrupt paths are enabled; this wording also
covers ESP-IDF, whose platform scheduler is already running in `app_main()`.

Unless a component states otherwise, public operations are task-context-only and
calls that reach the same physical peripheral are externally serialized. This
includes different logical objects that share one engine, such as two SPI devices
on one bus. Observation methods specifically documented as snapshot reads, such
as `Adc::latest()`, are safe against their backend ISR publisher. Methods
never retain a caller-owned span after a synchronous call returns.

### `error.hpp`

```cpp
namespace drivers {

enum class Error : std::uint8_t {
    invalid_argument,
    out_of_range,
    not_initialized,
    busy,
    timeout,
    no_data,
    buffer_too_small,
    write_protected,
    io,
    hardware_fault,
};

using Timeout = std::chrono::microseconds;
inline constexpr Timeout no_wait{0};

template <typename T>
class [[nodiscard]] Result {
public:
    static Result success(T value) noexcept;
    static Result failure(Error error) noexcept;

    [[nodiscard]] bool has_value() const noexcept;
    explicit operator bool() const noexcept;
    T& value() & noexcept;
    const T& value() const& noexcept;
    Error error() const noexcept;
};

template <>
class [[nodiscard]] Result<void> {
public:
    static Result success() noexcept;
    static Result failure(Error error) noexcept;

    [[nodiscard]] bool has_value() const noexcept;
    explicit operator bool() const noexcept;
    Error error() const noexcept;
};

} // namespace drivers
```

`Result` is a fixed-storage expected-like type. It does not allocate or throw.
Calling `value()` without a value is a contract violation handled by the project's
assertion policy. A peripheral missing from a target is rejected by CMake rather
than represented by a permanently failing runtime object.

`Timeout` is a maximum monotonic elapsed duration, not a FreeRTOS tick count.
`no_wait` performs one nonblocking attempt. Negative durations are
`invalid_argument`; finite positive durations include resource wait and transfer
completion as specified by the operation. Backends derive overflow-safe deadlines
from a target-owned monotonic hardware/platform time source and do not call
FreeRTOS. Application tasks use `no_wait` or reviewed short bounds when a long
synchronous wait would violate their worst-case execution or heartbeat budget.

### `gpio.hpp`

```cpp
namespace drivers {

class DigitalInput final {
public:
    [[nodiscard]] bool is_active() const noexcept;
};

class DigitalOutput final {
public:
    void set_active(bool active) noexcept;
    void toggle() noexcept;
};

} // namespace drivers
```

Direction, pull, output type, speed and active polarity are fixed by the target
binding. Separate input and output types prevent invalid operations. GPIO access
is infallible after successful system initialization; an interrupt-capable input
is added later as a separate semantic capability if required.

### `adc.hpp`

```cpp
namespace drivers {

enum class AdcReduction : std::uint8_t {
    none,
    block_mean,
    moving_mean,
};

struct AdcReductionConfig {
    AdcReduction kind;
    std::uint16_t window_samples;
};

struct AdcSample {
    std::uint32_t accumulated_raw;
    std::uint16_t sample_count;
    std::uint32_t generation;
};

class Adc final {
public:
    [[nodiscard]] std::uint32_t full_scale() const noexcept;
    [[nodiscard]] Result<AdcSample> latest() const noexcept;
};

} // namespace drivers
```

`Adc` is one logical sensor channel backed by a continuous scan engine.
`latest()` is non-destructive and returns `no_data` before the first complete
result. `accumulated_raw / sample_count` is the exact software-reduced mean, so
the driver never truncates a fractional ADC count. Its generation changes whenever
that channel publishes a new result, so task code can detect stale data without
making reads consume shared state.
Generation wraps modulo `std::uint32_t`; consumers test it for change rather than
ordering it with signed arithmetic.

`full_scale()` describes the post-shift numeric range returned by the configured
acquisition path; it must not assume that every backend always returns 12-bit
values. The interface returns ADC-domain counts. Voltage conversion, sensor
calibration, physical units, plausibility checks and vehicle-level filtering
belong to portable sensor logic above the driver.

`AdcReductionConfig` describes deterministic software reduction of continuous
samples. `window_samples` is one for `none`, the non-overlapping block size for
`block_mean`, and the rolling window size for `moving_mean`. A reduction policy
is fixed by the target composition and is not changed at runtime. Exponential,
median and domain-specific filters remain portable sensor components above the
driver rather than acquisition modes.

### `pwm.hpp`

```cpp
namespace drivers {

struct DutyCycle {
    std::uint16_t value; // 0 is 0%; 65535 is 100%
};

class Pwm final {
public:
    void set_duty(DutyCycle duty) noexcept;
    void enable() noexcept;
    void disable() noexcept;
};

} // namespace drivers
```

The target binding fixes frequency, polarity and alignment because multiple PWM
channels may share one hardware timer. If application-controlled frequency later
becomes necessary, expose the shared resource explicitly as a `PwmGroup` so a
frequency change cannot silently affect another channel.
Initialization leaves the output disabled at its configured safe electrical
level. `enable()` is used only after the application has published a valid
command and completed its startup checks.

### `serial.hpp`

```cpp
namespace drivers {

class Serial final {
public:
    [[nodiscard]] Result<std::size_t> write(
        std::span<const std::byte> data,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<std::size_t> read(
        std::span<std::byte> data,
        Timeout timeout) noexcept;
};

} // namespace drivers
```

`Serial` is a synchronous byte stream. The target binding normally fixes baud
rate, parity and stop bits. A successful read or write returns the number of bytes
transferred; a timeout is reported only when no further progress is possible by
the deadline. Queued logging and FreeRTOS synchronization belong in an adapter
above this contract.

### `spi.hpp`

```cpp
namespace drivers {

class SpiDevice final {
public:
    [[nodiscard]] Result<void> transfer(
        std::span<const std::byte> tx,
        std::span<std::byte> rx,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<void> write(
        std::span<const std::byte> data,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<void> read(
        std::span<std::byte> data,
        std::byte fill,
        Timeout timeout) noexcept;
};

} // namespace drivers
```

One object represents one configured SPI device, not an unowned controller. The
binding owns bus selection, chip select, mode, maximum clock and bus arbitration.
For `transfer`, TX and RX spans have equal size. A segmented-transaction type is
added only when a real peripheral requires command/address/data phases under one
chip-select assertion.
Chip select returns to its inactive level on success, timeout and I/O failure.
The backend recovers or resets the bus before accepting a later transaction after
an incomplete transfer.

### `i2c.hpp`

```cpp
namespace drivers {

class I2cDevice final {
public:
    [[nodiscard]] Result<void> write(
        std::span<const std::byte> data,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<void> read(
        std::span<std::byte> data,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<void> write_read(
        std::span<const std::byte> command,
        std::span<std::byte> response,
        Timeout timeout) noexcept;
};

} // namespace drivers
```

One object represents one configured 7-bit I2C target, not an unowned bus
controller. The target binding fixes the controller, target address, speed,
pin/pad configuration and arbitration policy. Address bytes are never passed by
application code, which prevents accidentally talking to a different device.

`write_read()` is one combined transaction with a repeated START between the
write and read phases; it is the normal register-read operation used by devices
such as the TLA2024. Empty phases are invalid. The implementation always emits a
STOP or performs bounded bus recovery before returning an error. Ten-bit
addresses, target/slave mode, SMBus features and runtime bus scanning are omitted
until a concrete requirement needs them.

The i.MX 8M Plus VCU binding will create two I2C device objects for TLA2024
addresses `0x49` and `0x48` if M7 owns I2C1. The ADC provider uses those
objects privately; application sensing tasks still depend on `Adc` or
`Adc` and never know the converter address. A53 and M7 must not both
program I2C1; cross-core serialization around two independent controller drivers
is not an accepted arbitration design.

### `can.hpp`

```cpp
namespace drivers {

enum class CanIdFormat : std::uint8_t {
    standard,
    extended,
};

struct CanFrame {
    std::uint32_t identifier;
    CanIdFormat format;
    std::uint8_t size;
    std::array<std::byte, 8> data;
};

enum class CanErrorState : std::uint8_t {
    error_active,
    error_passive,
    bus_off,
};

class Can final {
public:
    [[nodiscard]] Result<void> send(
        const CanFrame& frame,
        Timeout timeout = no_wait) noexcept;

    [[nodiscard]] Result<CanFrame> receive(
        Timeout timeout = no_wait) noexcept;
    [[nodiscard]] CanErrorState error_state() const noexcept;
};

} // namespace drivers
```

The backend validates identifier width and `size <= 8`. Filters and fixed receive
buffer capacity are target configuration. The same API maps to bxCAN, FDCAN in
classic mode, ESP32-S3 TWAI and the deterministic POSIX simulation. An optional
Linux-specific backend may map it to SocketCAN. Remote frames and target-specific
error counters are excluded from this contract.

`send()` succeeds when the frame has been accepted into backend-owned transmit
capacity; it does not promise that another node acknowledged it before returning.
The RX event is a wake-up hint and may coalesce several frames. The CAN RX task
therefore calls `receive(no_wait)` until it returns `no_data`, preserving frames
in the backend's fixed receive queue rather than assuming one notification per
frame.

### `can_fd.hpp`

```cpp
namespace drivers {

struct CanFdFrame {
    std::uint32_t identifier;
    CanIdFormat format;
    std::uint8_t size;
    bool bit_rate_switch;
    std::array<std::byte, 64> data;
};

class CanFd final {
public:
    [[nodiscard]] Result<void> send(
        const CanFdFrame& frame,
        Timeout timeout = no_wait) noexcept;

    [[nodiscard]] Result<CanFdFrame> receive(
        Timeout timeout = no_wait) noexcept;
    [[nodiscard]] CanErrorState error_state() const noexcept;
};

} // namespace drivers
```

`CanFd` accepts CAN FD data frames only. `size` is the decoded payload length,
not the protocol DLC, and must be one of 0..8, 12, 16, 20, 24, 32, 48 or 64.
`bit_rate_switch` requests the configured CAN FD data-phase rate for an outgoing
frame and reports it for a received frame. Nominal/data bit timings, message-RAM
allocation, filters and transceiver mode remain target-binding data. No target
may claim this component until its timing, error handling and 64-byte data paths
are accepted by target smoke and HIL testing.

### `watchdog.hpp`

```cpp
namespace drivers {

class Watchdog final {
public:
    [[nodiscard]] Result<void> start(
        std::chrono::milliseconds timeout) noexcept;
    void refresh() noexcept;
};

} // namespace drivers
```

There is deliberately no `stop()`: supported hardware may make the watchdog
irreversible once started. The actual achievable timeout is validated against the
target clock and watchdog limits during `start()`.

### `rtc.hpp`

```cpp
namespace drivers {

struct UtcTime {
    std::int64_t seconds_since_unix_epoch;
    std::uint32_t nanoseconds;
};

class Rtc final {
public:
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] Result<UtcTime> now() const noexcept;
    [[nodiscard]] Result<void> set(UtcTime time) noexcept;
};

} // namespace drivers
```

The RTC stores UTC; time zones, daylight saving and calendar formatting are
portable utilities above the driver. `is_valid()` reports whether retained time
survived according to the target's backup-power contract.

### `nv_memory.hpp`

```cpp
namespace drivers {

struct NvGeometry {
    std::size_t size_bytes;
    std::size_t write_alignment;
    std::size_t erase_alignment;
};

class NvMemory final {
public:
    [[nodiscard]] NvGeometry geometry() const noexcept;

    [[nodiscard]] Result<void> read(
        std::size_t offset,
        std::span<std::byte> destination) noexcept;

    [[nodiscard]] Result<void> write(
        std::size_t offset,
        std::span<const std::byte> source,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<void> erase(
        std::size_t offset,
        std::size_t size,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<void> sync(Timeout timeout) noexcept;
};

} // namespace drivers
```

This is raw persistent storage, not an atomic record database. Calibration record
versioning, CRC, redundancy and power-loss recovery belong in a higher-level
storage component. The backend owns flash unlock/erase/program sequences and wear
management required to provide the declared geometry.

### `block_device.hpp`

```cpp
namespace drivers {

struct BlockGeometry {
    std::uint32_t block_size;
    std::uint64_t block_count;
    bool read_only;
};

class BlockDevice final {
public:
    [[nodiscard]] BlockGeometry geometry() const noexcept;

    [[nodiscard]] Result<void> read_blocks(
        std::uint64_t first_block,
        std::span<std::byte> destination,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<void> write_blocks(
        std::uint64_t first_block,
        std::span<const std::byte> source,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<void> sync(Timeout timeout) noexcept;
};

} // namespace drivers
```

Buffer size must be an exact multiple of `block_size`; the block count is derived
from it. Filesystems, partition tables and removable-media policy remain above
this interface. `sync()` returns only after prior writes are stable according to
the selected medium, or returns `timeout`/`io` within the caller's bound.

### `ethernet_mac.hpp`

```cpp
namespace drivers {

struct MacAddress {
    std::array<std::byte, 6> bytes;
};

enum class LinkState : std::uint8_t {
    down,
    up,
};

class EthernetMac final {
public:
    [[nodiscard]] MacAddress address() const noexcept;
    [[nodiscard]] LinkState link_state() const noexcept;
    [[nodiscard]] std::size_t maximum_frame_size() const noexcept;

    [[nodiscard]] Result<void> transmit(
        std::span<const std::byte> frame,
        Timeout timeout) noexcept;

    [[nodiscard]] Result<std::size_t> receive(
        std::span<std::byte> destination,
        Timeout timeout) noexcept;
};

} // namespace drivers
```

This contract moves complete raw Ethernet frames using caller-owned buffers. PHY,
DMA descriptors, cache maintenance and network-stack integration remain private
or above the interface. A future zero-copy extension must specify ownership and
buffer lifetime explicitly; it must not silently change this contract.
`receive()` returns `buffer_too_small` without copying a partial frame; the
frame remains at the head of the receive queue so the caller can retry with a
buffer of `maximum_frame_size()`. Frames passed through this API include the
Ethernet header and payload but exclude preamble/SFD and FCS. The backend pads
short transmitted frames and generates/checks FCS where the selected MAC requires
it; oversize or malformed frames are rejected or counted as receive drops.

## Target configuration and mapping

There are two distinct kinds of configuration:

1. target-independent semantics, such as CAN bitrate, SPI mode, timeout, PWM
   frequency, active polarity, or ADC conversion mode;
2. physical binding, such as FDCAN1, GPIO port/pin, alternate function, IRQ,
   peripheral clock source, and DMA request.

Target-independent values use portable units and vocabulary. A value that the
application changes belongs in the public driver API; a value fixed for the life
of the firmware may be supplied once by the selected target binding. This does
not require runtime setters for every portable setting. Physical bindings and
fixed per-target choices belong in `config/<target>/driver_bindings.cpp`.

When a physical binding needs target-specific types, that source file is compiled
as part of the selected driver target and may include a private backend binding
header. The private include path must not propagate to application targets. This
is preferable to turning target details into supposedly portable configuration
types.

The application should not construct drivers from physical IDs. For a small ECU,
a `Hardware` aggregate of reference members adds more ceremony than value. A
smaller application-facing shape uses typed, semantically named objects:

```cpp
namespace hardware {

extern drivers::Can vehicle_bus;
extern drivers::Adc accelerator_primary;
extern drivers::Adc accelerator_secondary;
extern drivers::Pwm cooling_fan;
extern drivers::Watchdog watchdog;

drivers::Result<void> initialize();

using IsrEventCallback = void (*)(void* context) noexcept;
drivers::Result<void> connect_vehicle_bus_receive_event(
    IsrEventCallback callback,
    void* context) noexcept;
drivers::Result<void> enable_application_interrupts() noexcept;

} // namespace hardware
```

These objects have trivial construction and do not touch registers before
`hardware::initialize()`. Their definitions and physical bindings live in the
selected target configuration. Task code uses `hardware::vehicle_bus`, not a
physical identifier such as `FDCAN1`, `CAN_1`, or a family-specific CAN class.
Invalid or duplicated bindings should be detected at compile time when practical,
otherwise during `hardware::initialize()` before starting FreeRTOS. Initialization
errors must never be ignored.

The event connection is semantic rather than a generic public-driver callback.
The CAN RX task supplies a small ISR-safe callback only after its static task
handle exists; that callback performs `vTaskNotifyGiveFromISR()` and the required
yield. The selected target routes its CAN interrupt to this callback. Additional
event connections are added only when the application has a real interrupt-driven
consumer. This keeps FreeRTOS out of the driver API without forcing target IDs or
interrupt polling into task code. `enable_application_interrupts()` is a separate
startup gate so no callback can run against an incomplete application.

The three declarations have distinct jobs:

- `IsrEventCallback` is a non-owning function pointer. The `void*` carries a
  pointer to statically allocated caller context. Unlike `std::function`, this
  representation has fixed size, performs no allocation and has no hidden
  lifetime. `noexcept` states that an exception can never cross the ISR boundary.
- `connect_vehicle_bus_receive_event()` stores that callback/context pair in the
  selected target/backend's static CAN state. It validates non-null arguments and
  rejects rebinding after interrupts are enabled. It connects the event but does
  not enable the IRQ.
- `enable_application_interrupts()` clears stale peripheral/NVIC pending state and
  enables the application interrupt sources only after every callback, task
  handle and queue is ready. Keeping connection and enablement separate closes a
  startup race.

A CAN RX module can connect itself without dynamic allocation:

```cpp
namespace app::tasks::can_rx {
namespace {

struct IsrContext final {
    TaskHandle_t task{nullptr};
};

IsrContext isr_context;

void notify_from_isr(void* raw_context) noexcept {
    auto& context = *static_cast<IsrContext*>(raw_context);
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(context.task, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

} // namespace

drivers::Result<void> connect_receive_event(TaskHandle_t task) noexcept {
    if (task == nullptr) {
        return drivers::Result<void>::failure(drivers::Error::invalid_argument);
    }
    isr_context.task = task;
    return hardware::connect_vehicle_bus_receive_event(
        notify_from_isr,
        &isr_context);
}

} // namespace app::tasks::can_rx
```

The callback only wakes a task. It does not read frames, decode CAN messages,
log, allocate or refresh the watchdog in interrupt context. If CAN reception is
polled at a proven bounded period instead, this event connection is unnecessary;
the initial sample keeps it because interrupt-to-task wake-up is the normal ECU
CAN path.

### Binding implementation

The current FreeRTOS template distributes one mapping across several steps:

```text
X-macro driver list
        |
        v
semantic ID enum, for example GpioId::DEBUG_LED
        |
        v
target-specific X-macro associating the ID with a configuration record
        |
        v
backend-generated switch(id)
        |
        v
configuration pointer and opaque/backend state
```

This is functional and gives short construction syntax in task code, but the
same device concept participates in an ID list, target map and generated lookup.
The apparent brevity at the call site therefore moves complexity into several
files and retains a runtime ID-to-configuration translation.

The new architecture does not need that lookup. Exactly one target configuration
is compiled, so it constructs each semantically named, strongly typed object
directly. CMake performs target selection; C++ performs type checking; the target
binding performs physical composition.

A conceptual `config/stm32h563vit6/driver_bindings.cpp` looks like this:

```cpp
#include "hardware.hpp"

#include <array>
#include <cstdint>
#include <backend.hpp> // private include path of the selected backend only

namespace backend = drivers::stm32h5;

namespace {

std::uint64_t monotonic_microseconds() noexcept;

backend::CanStorage vehicle_bus_storage{
    .instance = backend::CanInstance::fdcan1,
    .receive_pin = {backend::Port::d, 0U, 9U},
    .transmit_pin = {backend::Port::d, 1U, 9U},
    .timing = {
        .prescaler = 10U,
        .sync_jump_width = 1U,
        .time_segment_1 = 13U,
        .time_segment_2 = 2U,
    },
    .now_us = monotonic_microseconds,
};

backend::AdcStorage accelerator_primary_storage{
    .reduction = {drivers::AdcReduction::moving_mean, 8U},
};

backend::AdcStorage accelerator_secondary_storage{
    .reduction = {drivers::AdcReduction::moving_mean, 16U},
};

const std::array<backend::AdcScanChannel, 2> accelerator_channels{{
    {{backend::Port::a, 1U, 0U}, 1U, 5U,
     &accelerator_primary_storage},
    {{backend::Port::a, 4U, 0U}, 18U, 5U,
     &accelerator_secondary_storage},
}};

backend::AdcScanStorage accelerator_scan{
    .instance = backend::AdcInstance::adc1,
    .channels = accelerator_channels.data(),
    .channel_count = accelerator_channels.size(),
    .oversampling = {.ratio = 16U, .right_shift = 4U},
};

} // namespace

namespace hardware {

drivers::Can vehicle_bus = backend::bind(vehicle_bus_storage);

drivers::Adc accelerator_primary =
    backend::bind(accelerator_primary_storage);

drivers::Adc accelerator_secondary =
    backend::bind(accelerator_secondary_storage);

drivers::Result<void> initialize() noexcept {
    if (auto result = backend::initialize(vehicle_bus_storage); !result) {
        return result;
    }
    if (auto result = backend::initialize(accelerator_primary_storage); !result) {
        return result;
    }
    if (auto result = backend::initialize(accelerator_secondary_storage); !result) {
        return result;
    }
    if (auto result = backend::initialize(accelerator_scan); !result) {
        return result;
    }
    return backend::start(accelerator_scan);
}

} // namespace hardware
```

The names and types in this example are illustrative backend-private APIs, not
public driver declarations. Equivalent files for the other target directories
provide the same `hardware` symbols with different physical bindings. CMake adds
only the selected directory and makes its backend binding header private.

This eliminates public driver-ID enums, duplicated ID lists, generated mapping
switches and macros in task code. It also makes the target binding useful as an
executable hardware map: reviewers can see the semantic role, physical instance,
pins and fixed settings together.

X-macros are not prohibited. A private X-macro may still be appropriate inside
one target configuration for a genuinely large homogeneous set, such as dozens
of identically configured digital outputs, or for a generated interrupt table.
It should remain a local code-generation convenience, not the public
application-to-hardware interface. A small heterogeneous ECU map is clearer when
written explicitly.

Interrupt dispatch and DMA ownership are registered by the backend during
`hardware::initialize()` and remain invisible to the application. FreeRTOS tasks
use the semantic objects after initialization and do not need to know how an IRQ,
DMA channel or peripheral instance was selected.

### Ownership boundaries

The target configuration owns:

- exact MCU and package selection;
- oscillator values and the global clock tree;
- voltage scaling, flash latency, cache/MPU policy, and TrustZone partitioning;
- pins and alternate functions;
- assignment of peripherals and DMA resources to semantic roles;
- linker script and startup policy.

The selected driver implementation owns:

- peripheral register programming;
- peripheral clock gate and reset sequencing;
- interrupt and DMA mechanics local to the peripheral;
- translation of portable configuration into register fields;
- peripheral state and error handling.

The application owns:

- ECU behavior and state machines;
- timeouts and recovery policy at system level;
- semantic names such as `vehicle_bus` or `accelerator_primary`.

### Binding and resource validation

A target binding is also the resource-ownership manifest for that firmware. A
reviewer must be able to determine from `config/<exact-target>/driver_bindings.cpp`
which semantic role owns every pin, peripheral, timer channel/trigger, DMA
request/channel, interrupt, CAN message-RAM region and persistent-memory region.
Shared ownership is forbidden unless it is expressed by one shared storage
object that creates non-owning views, as with several `Adc` channels on
one ADC scan engine or several SPI devices on one bus.

Backend-private binding descriptors are `constexpr` where practical. A private
validator is run with `static_assert` before the objects are created:

```cpp
constexpr auto target_plan = binding::make_target_plan(
    binding::can_resource(/* FDCAN1, pins, IRQ, message RAM */),
    binding::adc_scan_resource(/* ADC1, timer trigger, GPDMA, channels */),
    binding::pwm_resource(/* TIM channel and output pin */));

static_assert(binding::is_supported<binding::Device::stm32h563vit6>(target_plan));
static_assert(binding::resources_are_unique(target_plan));
```

This is an illustrative private API, not another public interface or a generated
application map. The same descriptors that pass validation are consumed to
construct the binding state; physical facts must not be copied into a second
manifest merely for validation.

The compile-time checks include, as applicable:

- exact package pin availability and alternate-function legality;
- conflicting GPIO, EXTI, peripheral, timer channel, trigger and DMA ownership;
- ADC sequence length, shared oversampling constraints and achievable sample
  throughput;
- PWM frequency/resolution feasibility from the selected timer clock;
- CAN nominal timing tolerance, filter capacity and FDCAN message-RAM layout;
- SPI/serial clocks and supported modes;
- interrupt priority compatibility with FreeRTOS;
- DMA access to the selected memory region and required alignment;
- nonvolatile/block-device region overlap and erase/block alignment;
- requested components against the exact-device capability table.

Facts that cannot be proved at compile time are checked in
`hardware::initialize()` before project tasks and application interrupts are
enabled. These include clock-lock status, peripheral calibration, external PHY
or storage presence when required, silicon identity/revision when readable and
ESP-IDF resource-allocation results. A mismatch returns a specific startup error
and leaves affected outputs in their documented safe state.

The target README records unavoidable physical sharing, external pull-ups,
transceivers, PHYs, voltage domains and board-level constraints that software
cannot infer. CMake validates target/backend/component compatibility; C++ binding
validation proves the concrete resource composition. Neither layer replaces the
other.

## The former `opaque` layer

The existing public `opaque_*.hpp` approach should not be applied to all drivers.
When those headers contain HAL handles, mapping pointers, or target-specific
state, they are not truly opaque: they expose implementation dependencies and
make object layout depend on the selected backend.

The replacement is:

- no public `opaque_*.hpp` headers;
- a small implementation-independent handle in the public object when state is
  required;
- backend state stored privately in the implementation;
- optional `backends/<target>/detail/` helpers for complex drivers only.

CAN may need private filter allocation, message RAM layout, timing calculations,
and IRQ routing. Those can live in `stm32h5/detail/` or `stm32h7/detail/` without
becoming a third API layer. A simple GPIO implementation probably needs only
`gpio.cpp`. Internal structure should follow actual complexity rather than a
mandatory pattern.

## Target capabilities and backend boundaries

The backend directory is selected by MCU family, while the exact device and
package are selected by the target configuration.

| Configuration target | Backend | Important distinctions |
| --- | --- | --- |
| STM32F446VET6 | `stm32f4` | Cortex-M4F, two classic bxCAN controllers, legacy DMA, three 12-bit ADCs, no integrated Ethernet MAC |
| STM32H563VIT6 | `stm32h5` | Cortex-M33 with TrustZone and caches, two FDCAN controllers, GPDMA, two 12-bit ADCs, two SDMMC interfaces and an integrated Ethernet MAC |
| STM32H723VGT6 | `stm32h7` | Cortex-M7, three FDCAN controllers, Ethernet MAC, cache/DMA coherency concerns, two 16-bit ADCs and one 12-bit ADC |
| ESP32-S3-WROOM-1-N16R8 | `esp32s3` | Dual-core Xtensa LX7, 16 MB Quad-SPI flash, 8 MB Octal-SPI PSRAM, Wi-Fi and Bluetooth LE, one classic-CAN-compatible TWAI controller, no CAN FD and no integrated Ethernet MAC |
| Verdin iMX8M Plus Quad 4GB WB IT / VCU v9 M7 | `imx8mp-m7` | Cortex-M7 sharing the SoC with Linux/A53; two native FlexCAN controllers, external MCP2518FD and two external TLA2024 converters; cross-core ownership is mandatory |
| POSIX host | `posix` | deterministic simulated devices; an optional Linux extension may use SocketCAN |

The initial component capability matrix uses these symbols:

- `N`: native on-chip capability implemented by the backend;
- `S`: software-backed, emulated or simulated implementation;
- `E`: external hardware is required and must be declared by the target;
- `P:`: planned physical path whose production implementation is still
  fail-closed; the suffix states whether that path is native or external;
- `-`: no implementation in the initial backend.

| Public component | F446VET6 | H563VIT6 | H723VGT6 | ESP32-S3 N16R8 | i.MX8MP M7 | POSIX |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| GPIO | N | N | N | N | P:N | S |
| ADC/on-demand | N | N | N | N | P:E | S |
| ADC continuous/reduction | N+S | N | N | N+S | P:E+S | S |
| PWM | N | N | N | N | P:N | S |
| Serial | N | N | N | N | P:N | N/S |
| SPI controller/device binding | N | N | N | N | P:N | S |
| I2C target binding | - | - | - | - | P:N | S |
| Classic CAN | N+E | N+E | N+E | N+E | P:N+E | S |
| Watchdog | N | N | N | N | P:N | S |
| RTC | N | N | N | E | P:N | N/S |
| Nonvolatile memory | S | S | S | S | - | S |
| Block device | N+E | N+E | N+E | N+E | - | S |
| Ethernet MAC | E | N+E | N+E | E | - | S |

All embedded CAN entries require an external transceiver. STM32 RTC retention
requires the target's VBAT/backup-power and clock-source assumptions to be met.
The STM32 block-device entries use an on-chip SDIO/SDMMC or SPI controller but
still require routed pins and storage media. The H563/H723 Ethernet entries have
an on-chip MAC but require an external PHY and validated RMII/MII wiring. F446 and
ESP32-S3 can expose `EthernetMac` only through a supported external controller
that provides the required raw-frame semantics.

The M7 entries are intentionally `P:` rather than support claims. Linux
initially owns Ethernet, microSD, USB, HDMI and its diagnostic UART. M7 initially
targets GPIO, PWM, UART1/UART4, ECSPI1, FlexCAN1/2 and WDOG. Its MCP2518FD CAN
path and TLA2024 ADC path are external providers inside the same selected
backend. I2C1 and SNVS RTC remain gated until cross-core ownership is explicit.

The F446 has no hardware oversampling unit in this design; its continuous ADC
backend uses timer/DMA acquisition and deterministic software reduction. H5/H7
may use their hardware oversampling engines. ESP32-S3 may use its continuous ADC
and available digital filtering internally, but software supplies any exact
portable mean-window contract that the hardware cannot match. POSIX CAN is a
deterministic simulation in the portable backend; SocketCAN is enabled only by a
separately named Linux backend or extension.

For the initial interface set, the ESP32-S3-WROOM-1-N16R8 target can provide
GPIO, ADC, PWM, serial, SPI, classic CAN through TWAI, watchdog, flash-backed
nonvolatile memory and supported SD/SPI block devices. It does not implement CAN
FD or an on-chip Ethernet MAC. It implements `Rtc` only when the target includes
hardware that satisfies the public calendar-retention contract, such as an
external battery-backed RTC; an uptime counter or sleep-retained timer alone is
not sufficient.

The `MON16R8` text reported on the module is recorded as its marking; the official
module ordering code used by configuration and documentation is
`ESP32-S3-WROOM-1-N16R8`. In that code, `N16R8` identifies 16 MB Quad-SPI flash
and 8 MB Octal-SPI PSRAM.

Family membership is not sufficient evidence that a feature is present. Every
binding must be validated against the exact part number and package. Family-level
backend selection never replaces exact-device capability validation.

Notable backend differences must remain private:

- STM32F4 CAN uses bxCAN mailboxes and filters; H5/H7 use FDCAN message RAM;
- ESP32-S3 uses one TWAI controller and requires an external CAN transceiver;
- F4, H5, and H7 DMA controllers have materially different programming models;
- ESP32-S3 is an Xtensa/ESP-IDF platform rather than an Arm/CMSIS platform;
- ADC resolution, calibration, triggering, and sequencing differ by family;
- H563 and H723 caches require explicit DMA buffer placement or cache
  maintenance;
- the initial H563 configuration is a TrustZone-disabled single image; any future
  secure/non-secure split is a new target architecture and provisioning change;
- interrupt names and clock muxes vary by exact device.

These differences are a reason for separate implementation targets, not a reason
to leak family-specific types into the common interface.

Hardware capability claims in this document are based on the manufacturers'
official documentation:

- [STM32F446xC/E datasheet](https://www.st.com/resource/en/datasheet/stm32f446mc.pdf)
- [STM32H563xx datasheet](https://www.st.com/resource/en/datasheet/stm32h563vi.pdf)
- [STM32H723xE/G datasheet](https://www.st.com/resource/en/datasheet/stm32h723vg.pdf)
- [ESP32-S3 series datasheet](https://documentation.espressif.com/esp32-s3_datasheet_en.pdf)
- [ESP32-S3-WROOM-1/WROOM-1U module datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
- [ESP32-S3 TWAI documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/twai.html)
- [ESP-IDF build-system documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/build-system.html)
- [ESP-IDF FreeRTOS documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/freertos_idf.html)

The relevant technical/reference manual and current device errata must also be
reviewed before each embedded implementation is considered complete.

## Build-system contract

The user selects one target configuration, for example:

```text
-DECU_CONFIG=stm32h563vit6
```

The configuration CMake file derives the exact device, backend, platform support
settings and required components. Users must not independently select an MCU and
an unrelated driver backend.

A driver-library entry point can have the following conceptual form:

```cmake
drivers_add_library(ecu_drivers
    BACKEND stm32h5
    DEVICE STM32H563xx
    COMPONENTS
        gpio adc pwm serial spi can watchdog rtc
        nv_memory block_device ethernet_mac
    CONFIG_SOURCES ${CMAKE_CURRENT_LIST_DIR}/driver_bindings.cpp
)

target_link_libraries(ecu_firmware PRIVATE ecu_drivers)
```

`drivers_add_library` should:

- validate the backend/device pair;
- validate every requested component against exact-device capabilities or an
  explicitly named external provider declared by the target;
- add exactly one family target, such as `ecu_drivers_stm32h5`, and link it
  privately into the selected `ecu_drivers` composition;
- compile only the selected implementation sources;
- expose only `drivers/include` publicly;
- expose backend and platform-SDK headers privately;
- define the exact CMSIS device macro privately for STM32 targets where possible;
- fail immediately for missing source files or unsupported components.

An external provider is concrete hardware, not a boolean override. For example,
a future F446 target may name a supported external raw-frame Ethernet controller
and its SPI/interrupt/reset resources. Without that provider, requesting
`ethernet_mac` fails even though `E` appears in the capability matrix. The target
binding and its validator then prove the provider's bus and pin composition.

The application should receive one link target, `ecu_drivers`, rather than know
which family libraries compose it.

For ESP32-S3, `ecu-drivers-esp32s3` also provides an ESP-IDF component entry
point named `ecu_drivers_esp32s3` using `idf_component_register`. The
`esp32-s3-wroom-1-n16r8`
configuration selects `IDF_TARGET=esp32s3`, pins the supported ESP-IDF/toolchain
release, configures 16 MB Quad-SPI flash and 8 MB Octal-SPI PSRAM, and supplies
the target's `sdkconfig` defaults. The application still includes only the flat
public headers from `include/`; ESP-IDF component names and types do not cross
the public driver boundary.

For the M7 target, `ecu-drivers-imx8mp-m7` defines
`ecu_drivers_imx8mp_m7` and requires device identifier `MIMX8ML8_cm7`.
The firmware configuration, not a user-selected second option, derives that
pair. CMake recognizes the pair now but keeps every component disabled until its
source and acceptance evidence exist. This makes a newly checked-out backend
directory visible without turning scaffolding into a support claim.

## Language, toolchain and reproducibility contract

Project-owned C++ is C++20 with extensions disabled. C sources are C17. C++20 is
required for `std::span`, designated initializers used by configuration code and
the expected compile-time facilities. The baseline build requires CMake 3.25 or
newer and uses targets rather than directory-global include paths, definitions or
options.

Embedded project code is compiled with:

- exceptions disabled;
- RTTI disabled;
- no use of `std::iostream`, locale, filesystem or heap-owning standard
  containers in firmware paths;
- function and data sections with linker garbage collection;
- strict warnings for conversions, shadowing, format strings and undefined
  preprocessor symbols;
- no `-ffast-math` or other flags that weaken floating-point or NaN semantics;
- debug information and `-Og` for debug builds, and a reviewed `-O2` or size
  optimization for release builds.

Warnings in project-owned code are errors in CI. Vendor headers and pinned
upstream code are compiled as system/external targets so their warnings are not
silenced globally. Link-time optimization is an optional release preset and must
pass the same target tests; correctness must not depend on it.

`CMakePresets.json` defines, at minimum, host-debug, host-sanitized and one preset
per embedded target. Target CPU, FPU, ABI and linker flags are set only by the
selected target toolchain/configuration. In particular:

- STM32F446VET6 uses the Cortex-M4F hard-float ABI;
- STM32H563VIT6 uses the Cortex-M33 hard-float ABI and the initial single-image,
  TrustZone-disabled configuration;
- STM32H723VGT6 uses the Cortex-M7 hard-float ABI with its target cache/memory
  policy;
- ESP32-S3 obtains Xtensa flags exclusively from the pinned ESP-IDF toolchain.

The build must not silently program STM32 option bytes, change H5 TrustZone state,
enable ESP secure boot, enable flash encryption or alter a partition table. Such
operations require a separately reviewed provisioning procedure and must agree
with the target README.

Versions are exact pins, not open ranges:

- the Git submodule commits pin FreeRTOS, driver backends and CMSIS packages;
- a version file records the exact Arm GNU Toolchain release and checksum;
- the ESP configuration records an exact ESP-IDF tag and commit, which in turn
  owns its compiler toolchain;
- CI records the exact host compiler and CMake versions;
- dependency updates are isolated changes that rebuild and retest every target.

Normal CMake configuration performs no network download. A separate documented
bootstrap command initializes recursive submodules and verifies toolchain
checksums. Source lists are explicit; filesystem globbing is not used for
production targets.

Every release build emits ELF, binary/hex where applicable, linker map, symbol
size report and a machine-readable manifest containing the firmware commit,
recursive submodule commits, target, build preset, compiler version and relevant
configuration hashes. CI archives these files together so a deployed image is
traceable and reproducible.

## Direct-register implementation policy

The STM32 and i.MX 8M Plus M7 backends may be written without their vendor
peripheral HALs. They should still use the official CMSIS core and exact-device
headers for register layouts, addresses, IRQ numbers, intrinsics, barriers, and
core definitions. Rewriting vendor register maps provides no architectural
benefit and increases the chance of incorrect reserved bits or offsets.

The boundary is:

- CMSIS core/device definitions are allowed and private;
- STM32 HAL/LL and MCUXpresso `fsl_*` peripheral APIs and handle types are
  not used;
- peripheral behavior is implemented from the reference manual, device data
  sheet, and current errata;
- register writes use explicit masks and document non-obvious sequencing;
- waits have bounded timeouts;
- ISRs do minimal work and never allocate;
- clock, reset, and interrupt assumptions are documented beside the driver.

System-wide initialization does not belong in a peripheral driver. PLL setup,
CPU clock selection, flash latency, power scaling, caches, MPU, TrustZone, startup
memory initialization, and the linker script remain target-configuration
responsibilities.
Peripheral drivers may enable/reset their own peripheral clock and consume clock
frequencies supplied by the target configuration.

On i.MX 8M Plus, clock/reset/IOMUX ownership is also a cross-core contract.
Linux, TF-A/U-Boot and remoteproc may establish state before M7 starts. An M7
driver validates the expected handoff and changes only resources assigned to it;
it does not reset a shared clock domain or reprogram a pad owned by Linux.
MCUXpresso SDK sources may be read as vendor sequencing references, but only
CMSIS/device definitions, reviewed startup/linker material and licenses are
compiled into project targets.

Because direct-register drivers trade vendor abstraction for control, each new
driver is complete only when it has:

- a reference-manual-based implementation;
- timeout and error paths;
- interrupt behavior documented and tested;
- a minimal target smoke test;
- checks for the exact supported device variants;
- cache and memory-order handling where DMA is involved.

### ESP32-S3 implementation policy

The ESP32-S3 backend should use ESP-IDF privately rather than attempt a complete
register-level reimplementation. Wi-Fi, Bluetooth LE, RF calibration, flash,
boot, coexistence and security support are platform services whose replacement
would be a separate, high-risk project rather than a driver backend.

The backend may use lower-level ESP-IDF HAL or register access for an isolated
peripheral only when resource ownership is unambiguous and the implementation
does not conflict with ESP-IDF. A peripheral must not be controlled partly by an
ESP-IDF driver and partly by project register code at the same time.

The no-dynamic-allocation rule applies to project-owned driver wrappers and
their state. ESP-IDF networking and radio components may allocate internally;
their memory use must be bounded through configuration, measured and included in
the target's memory budget if those services are enabled.

## Memory, DMA and cache contract

Embedded project-owned firmware uses static storage after startup. Driver state,
task stacks, task control blocks, queues, DMA buffers and protocol buffers have
compile-time capacity. The STM32 FreeRTOS configurations set static allocation on
and dynamic allocation off. C++ `new`/`delete` and heap-owning containers are not
used in embedded application or driver paths. ESP-IDF system components may use
their managed heaps, but project-owned tasks and queues remain statically
allocated and no project code allocates during steady-state operation.

Every STM32 linker script defines and exports at least:

- executable/read-only flash;
- initialized and zero-initialized RAM;
- a `.noinit` region for explicitly retained diagnostic state;
- one or more `.dma` regions with documented alignment and bus accessibility;
- task/interrupt stack bounds and a zero-sized or deliberately bounded C heap;
- symbols used by startup code and link-time memory-budget checks.

Objects enter `.noinit` only through an explicit attribute and must contain a
magic, format version and integrity check before use. Ordinary C++ objects with
constructors never live there. Reset-cause diagnostics may be retained; secrets
and stale actuator commands may not.

DMA buffers are static, naturally aligned for the peripheral transfer and aligned
to the target cache-line requirement when caches exist. A buffer is owned by one
driver operation until completion; application code cannot mutate it while DMA
owns it. Public APIs use spans whose lifetime requirements are documented and do
not retain caller buffers beyond the documented synchronous call unless the API
explicitly transfers ownership.

Target rules are:

- STM32F446VET6 DMA buffers live in DMA-accessible SRAM, never in core-coupled
  memory that the DMA bus cannot reach.
- STM32H563VIT6 DMA buffers live in a GPDMA-accessible region. The target either
  maps that region non-cacheable or the backend performs direction-correct cache
  clean/invalidate operations and barriers.
- STM32H723VGT6 DMA buffers do not live in DTCM unless the exact DMA path can
  access it. Prefer a dedicated non-cacheable MPU region in DMA-accessible SRAM;
  otherwise use reviewed cache maintenance rounded to complete cache lines.
- ESP32-S3 DMA and ISR-shared data use internal DMA-capable memory. The initial
  configuration does not place task stacks, DMA buffers or ISR-required state in
  PSRAM. PSRAM is reserved for explicitly bounded, non-real-time bulk data.

Cache maintenance occurs at ownership boundaries, not as an incidental action in
application code. Transmit buffers are cleaned before DMA reads them; receive
buffers are invalidated after DMA completion and before CPU consumption. Required
memory barriers are part of the backend implementation. Cache-line constants and
memory regions come from the selected target, never from application literals.

CI parses every linker map and fails on region overflow. Each target README
records release flash/RAM use and budgets for static data, task stacks, DMA
buffers and remaining headroom. Stress testing must leave at least 25 percent of
each project task stack unused at its measured high-water mark; a task that does
not meet this rule gets a reviewed stack increase or is redesigned.

## Startup, initialization and shutdown contract

The STM32 reset-to-scheduler sequence is fixed:

```text
reset handler
    -> mask application peripheral interrupts
    -> initialize stack, data and BSS
    -> establish FPU/core state
    -> platform::early_initialize()
       (power/flash/clock, cache/MPU, reset cause, safe pin policy)
    -> run C++ static initialization
    -> main()
    -> hardware::initialize()
    -> app::initialize()
       (context, queues, blocked worker tasks, ISR binding, supervisor last)
    -> vTaskStartScheduler()
    -> supervisor startup gate
       (application interrupts, watchdog handoff, release worker tasks)
```

Static constructors are trivial and do not access peripherals. The reset handler
and `platform::early_initialize()` have no dependency on FreeRTOS or dynamically
initialized C++ objects. `main()` must never return. If the scheduler returns,
the firmware enters the fatal-fault path.

The top-level control flow stays explicit:

```cpp
int main() {
    if (const auto result = hardware::initialize(); !result) {
        platform::fatal(Fault::hardware_initialization);
    }
    if (!app::initialize()) {
        platform::fatal(Fault::application_initialization);
    }

    vTaskStartScheduler();
    platform::fatal(Fault::scheduler_returned);
}
```

The ESP-IDF `app_main()` performs the same two checked calls but does not start or
stop the scheduler. The POSIX entry point reports a nonzero result instead of
resetting the host. These are small platform entry points, not alternate
application implementations.

Before any actuator pin is switched to output or alternate-function mode, its
inactive electrical level is written to the output data register. PWM outputs
remain disabled and communication transceivers remain in their documented safe
state until their complete configuration succeeds. The safe state of every
actuator is recorded in the target README and represented directly in its
binding.

`hardware::initialize()` initializes physical engines once, not once per logical
channel. Dependency order is explicit: clocks and pins precede peripherals;
shared ADC/SPI/CAN engines precede their channel/device views; DMA and IRQ state
is cleared before enabling requests; pending NVIC state is cleared immediately
before an interrupt is enabled.

`app::initialize()` follows the SMU application style in ordered gates:

1. create the static `AppContext`, its startup event and safe initial snapshots;
2. create all static queues and other RTOS objects;
3. create all worker tasks and register their heartbeat bits; the first action of
   each worker is to block on the startup event;
4. connect ISR callbacks/notifications only after the receiving task handles
   exist;
5. verify that every required subsystem is ready;
6. create the supervisor last;
7. return to `main()`, which starts the scheduler.

The name is intentionally `initialize`, not `start`: on STM32 it creates static
FreeRTOS objects and places tasks in the kernel's ready lists, but it does not run
their entry functions. FreeRTOS explicitly permits static task/queue creation
before `vTaskStartScheduler()`. The initialization path must not call a blocking
delay, wait for a task to execute or assume that a notification will be consumed.
When `vTaskStartScheduler()` is called, FreeRTOS performs port/tick setup and
dispatches the highest-priority ready task, which is the supervisor in this
design. Therefore the application is fully constructed before scheduling, but
its concurrent behavior starts only with the scheduler.

The supervisor is the only project task that does not initially wait on the
startup event. Its first execution enables the already-connected application
interrupt paths, arms or accepts handoff of the watchdog, and then publishes the
startup event. Only after that event do worker tasks enter their normal loops.
The supervisor then becomes the periodic heartbeat/watchdog task. Failure in any
of these final gates enters the fatal path without releasing the workers.

No project task can execute before the STM32 scheduler starts. On ESP-IDF the
scheduler already exists when `app_main()` is called. A newly created ESP worker
may therefore run immediately, but it can only block on the pre-created startup
event. The supervisor is still created last and releases the workers only after
the same final gates. POSIX uses `main()` and the FreeRTOS POSIX scheduler like
STM32.

The template has no general runtime shutdown. An embedded fatal fault attempts
to command safe outputs using already initialized, nonblocking paths, records a
bounded diagnostic snapshot and stops normal watchdog servicing. If the watchdog
is armed, it performs the reset; otherwise the target requests a controlled
system reset after recording the failure, or remains in a safe halted state when
reset is unavailable during debug. It does not run C++ global destructors. POSIX
may stop the scheduler and return a nonzero process result for integration tests.

## Fault, assertion and reset policy

Failures are classified as:

- configuration/build failures: unsupported components, duplicate resources,
  invalid clocks or incompatible versions; these stop CMake or compilation;
- startup-fatal failures: required clock, memory, driver, queue, task or ISR-path
  initialization failed; application tasks are not started;
- recoverable runtime failures: transient busy, timeout, no-data or bus errors;
  the owning task performs a bounded retry or enters a documented degraded state;
- fatal runtime failures: corrupted invariants, repeated recovery failure, stack
  overflow, scheduler failure or a required task heartbeat missing; outputs are
  made safe and watchdog servicing stops.

All fallible return values are `[[nodiscard]]`. Ignoring an error requires an
explicit cast and a comment explaining why. Retry loops have a count or deadline;
there are no unbounded retry/busy loops in normal operation.

`configASSERT` and project contract checks remain enabled in debug and release.
Debug builds break into the debugger when attached. Release builds route a
contract violation to a minimal, nonallocating fatal handler that records file or
numeric site information when safe, disables offending interrupt sources and
requests watchdog reset. Stack-overflow, malloc-failure where an upstream platform
can allocate, hard-fault and unhandled-interrupt hooks use the same terminal path.

Reset cause is captured before peripheral flags are cleared and exposed to the
diagnostic application state. Repeated watchdog resets are not hidden; a
bootloader or higher-level policy may enter recovery after a defined threshold.
The firmware architecture is safety-oriented but is not claimed to be compliant
with a functional-safety standard without the separate process, analysis,
traceability and evidence that standard requires.

## RTOS and concurrency policy

The driver library is operating-system independent and does not include or call
FreeRTOS. The ECU application is intentionally FreeRTOS-specific: it uses the
FreeRTOS task, queue and notification APIs directly, following the established
SMU style. The template does not introduce `IRtos`, `ITask`, `IQueue`, an event
framework, or a second scheduler abstraction.

Drivers are initialized once by the startup path. Tasks use the semantic objects
in `hardware.hpp`; they never initialize a peripheral, bind an interrupt, or
select a physical instance. On STM32 and POSIX, project tasks cannot run until
the scheduler starts. On ESP-IDF, where the scheduler is already running before
`app_main()`, all ECU task creation and application interrupt enablement remain
behind the ordered startup gates described above.

### Application context and task modules

The application has one statically allocated `app::AppContext`. It contains
fixed-capacity queues, published state snapshots and task-coordination state. It
must not become a service locator for every driver: hardware access is expressed
by the narrow semantic objects in `hardware.hpp` and task ownership is documented
explicitly.

Each task is a small module under `app/tasks/` with the same shape:

- one `create_task(AppContext&)` entry point;
- one private FreeRTOS task entry function;
- a statically allocated `StaticTask_t` and `StackType_t` array;
- a named priority, stack capacity and heartbeat bit;
- no public task class, dynamic allocation, or global constructor;
- a task body that never returns.

A representative periodic task is:

```cpp
namespace app::tasks::sensing {
namespace {

constexpr UBaseType_t priority = 4;
constexpr std::size_t stack_elements = 512;
constexpr TickType_t period = pdMS_TO_TICKS(5);
static_assert(period > 0);

StaticTask_t task_control_block;
StackType_t task_stack[stack_elements];

void task_entry(void* argument) {
    auto& context = *static_cast<AppContext*>(argument);

    (void)xEventGroupWaitBits(
        context.start_event,
        AppContext::started_bit,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY);

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        const auto primary_result = hardware::accelerator_primary.latest();
        const auto secondary_result = hardware::accelerator_secondary.latest();

        if (!primary_result) {
            context.health.report_adc_error(
                AdcRole::accelerator_primary,
                primary_result.error());
        }
        if (!secondary_result) {
            context.health.report_adc_error(
                AdcRole::accelerator_secondary,
                secondary_result.error());
        }

        if (primary_result && secondary_result) {
            const auto& primary = primary_result.value();
            const auto& secondary = secondary_result.value();
            context.sensing.publish(
                primary,
                secondary);
            context.health.observe_adc_generations(
                primary.generation,
                secondary.generation);
        }

        context.heartbeat.report(TaskHeartbeat::sensing);
        vTaskDelayUntil(&last_wake, period);
    }
}

} // namespace

TaskHandle_t create_task(AppContext& context) noexcept {
    return app::freertos::create_static_task(
        task_entry,
        "sensing",
        task_stack,
        &context,
        priority,
        task_control_block);
}

} // namespace app::tasks::sensing
```

The helper in this example is deliberately small. It normalizes the
stack-capacity argument between upstream FreeRTOS and ESP-IDF and returns the
native `TaskHandle_t`; the startup gate checks it for failure. It does not hide
FreeRTOS semantics. This normalization is required because upstream
`xTaskCreateStatic()` expresses stack depth in `StackType_t` entries while
ESP-IDF's API expresses it in bytes. Tasks continue to use `TickType_t`, queues,
notifications, and `vTaskDelayUntil()` directly.

The example performs no allocation, has a compile-time stack and nonzero period,
checks both `Result` objects before calling `value()`, and executes a fixed amount
of work per period. `observe_adc_generations()` applies a configured freshness
deadline; one unchanged generation is not automatically a fault because the ADC
publication rate may be slower than the task. The heartbeat means that the task
completed its bounded health check, while `context.health` separately records
whether the sensors are healthy. A task must not hide a bad sensor by withholding
all application diagnostics until the watchdog resets it.

The initial sample application uses this ownership model:

| Task | Trigger | Owned operation | Initial relative priority |
| --- | --- | --- | ---: |
| supervisor | startup, then fixed period | release startup gate, validate heartbeats and refresh watchdog | 6 |
| CAN receive | driver event/notification | drain CAN RX and publish decoded state | 5 |
| sensing | 5 ms period | read ADC snapshots and run sensor checks | 4 |
| CAN transmit | queue/event | serialize and transmit application frames | 3 |
| diagnostics/serial, if enabled | queue/event | emit bounded diagnostic records | 2 |
| status LED | fixed period/event | indicate the published system state | 1 |

These are reference priorities, not unexplained magic values. The selected
`FreeRTOSConfig.h` must provide enough priority levels and the target timing
analysis may change them. Priorities encode latency requirements only; they are
not used to repair shared-state races. Every task stack and queue capacity is a
named constant with a measured justification in the target README.

Periodic tasks use `vTaskDelayUntil()` so execution time does not accumulate as
phase drift. Event-driven tasks block on a notification or queue. There are no
polling loops without a delay and no `portMAX_DELAY` wait in a task that must
report a periodic heartbeat, unless another independently verified mechanism
reports that task's liveness. The one deliberate exception is the worker's
pre-start wait, which occurs before watchdog heartbeat supervision begins.
Frequency-to-tick conversion is checked at compile time where constant and must
never silently produce zero ticks.

### Data ownership and synchronization

Prefer single-writer ownership and message passing:

- the sensing task is the only writer of processed sensor state;
- the CAN RX task is the only consumer of received frames;
- the CAN TX task is the only task that calls the transmit path for the vehicle
  bus; other tasks enqueue fixed-size transmit requests;
- the supervisor is the only task that refreshes the watchdog;
- an SPI bus has one owning task, or all of its device operations are protected
  by one explicitly documented task-context mutex.

Use a task notification for a counted wake-up from one known source. Use a
fixed-capacity queue when a payload, ordering, or multiple producers matter. Use
an event group only for level-like state combinations. Mutexes are a last resort
for genuinely shared task-context resources; recursive mutexes are not used.

Published state uses a working copy and a short, coherent snapshot publication.
A critical section may copy bounded plain data or swap a generation index. It
must not call a driver, wait, format text, perform software averaging, or execute
protocol logic. `volatile` is not a synchronization mechanism. Atomics are used
only when their lock-free behavior and required ordering are known on every
selected target; otherwise use a FreeRTOS critical section or message passing.

Every public driver operation is task-context-only in the initial contract. A
driver documents whether concurrent task calls are internally serialized or
require external serialization; the default is external serialization. This
conservative rule avoids making every portable interface carry ISR semantics.
If a later requirement needs a public ISR-callable operation, it is added as an
explicitly named operation with its own nonblocking contract.

### Interrupt-to-task path

Backend ISRs acknowledge the hardware, capture the minimum bounded state, and
notify a task through a pre-registered function pointer plus context or a
backend-owned fixed event record. The application side uses only FreeRTOS
`...FromISR` APIs and requests a context switch with `portYIELD_FROM_ISR()` when
required. The callback is connected only after its receiving task handle and
queue exist, and disconnected or masked before their lifetime can end.

An ISR must not allocate, block, format/log text, use floating point, run a state
machine, refresh the watchdog, or call a task-only driver method. ISR-to-task
queues have a defined overflow policy and an observable dropped-event counter;
overflow is never silently ignored.

A fixed-cost backend operation that is intrinsic to acquisition, such as updating
a running ADC sum for a compile-time-bounded scan, is permitted only when its
worst-case interrupt time is measured and accepted for every target. Vehicle
logic, unit conversion, plausibility checks and general filtering never use this
exception.

Each target derives NVIC priorities from symbolic application levels and the
implemented priority-bit count. Any ISR that calls FreeRTOS must be at a priority
permitted by `configMAX_SYSCALL_INTERRUPT_PRIORITY`. A more urgent ISR may exist,
but it must not call FreeRTOS. Target startup validates the grouping and relevant
priorities before enabling interrupts; raw priority literals do not appear in
individual peripheral drivers.

### Watchdog and liveness

Each required task owns one heartbeat bit. It reports the bit only after
completing its useful bounded unit of work, not merely on entering its loop. At a
fixed supervision interval, the supervisor atomically consumes the reported
mask, compares it with the required mask, records missing tasks and refreshes the
watchdog only when every required task and critical subsystem is healthy.

The supervision interval is longer than the maximum required-task heartbeat
period plus measured scheduling margin. An event-driven task uses a bounded wait
and may report after checking that its queue/driver state is healthy even when no
external event arrived. The first comparison occurs only after one complete
supervision interval following startup release; it is not a permanent boot grace
period. A genuinely slower task uses an explicitly reviewed per-task deadline,
not repeated exceptions to a global missing-bit check.

The watchdog is the final startup gate. Its timeout must exceed the measured
worst-case startup/handoff time and at least two supervisor periods with reviewed
margin. No driver ISR, idle hook, timer callback, or non-supervisor task refreshes
it. If an earlier boot stage has already enabled a watchdog, the target startup
documents and implements an explicit checkpoint handoff instead of temporarily
weakening supervision.

### FreeRTOS configuration baseline

The STM32/POSIX `FreeRTOSConfig.h` files and equivalent ESP-IDF `sdkconfig`
settings enforce these project policies:

- `configSUPPORT_STATIC_ALLOCATION == 1`;
- `configSUPPORT_DYNAMIC_ALLOCATION == 0` on STM32 and the FreeRTOS POSIX test
  target;
- stack-overflow checking at the strongest supported portable level;
- `configASSERT` enabled in every build;
- at least seven task-priority levels for the reference priority plan;
- a 1 kHz tick unless target timing analysis documents another choice;
- no project dependency on software timers until a concrete feature needs them;
  ESP-IDF may retain timers required by its platform components;
- idle and timer hook work, if enabled, bounded and nonblocking;
- runtime stats and trace facilities optional in debug/instrumented presets, not
  required for correctness.

The initial ESP32-S3 configuration sets `CONFIG_FREERTOS_UNICORE=y`. This gives
project logic the same single-core reasoning model as the STM32 targets while
ESP-IDF may still run platform work according to its documented configuration.
Project tasks use static allocation and the portable FreeRTOS API subset. They do
not use affinity APIs or assume the vanilla FreeRTOS stack-size unit. Enabling
SMP later is an architectural change requiring a new race, ISR, cache, timing and
watchdog review; it is not a one-line performance option.

The POSIX configuration runs the same `app/` task modules through a pinned
FreeRTOS POSIX port for integration and contract tests. It validates task logic,
ownership and startup ordering, but it is not evidence of embedded interrupt
latency or hard real-time behavior.

## Driver-specific guidance

### CAN

- Present one public `Can` interface for classic CAN.
- Present `CanFd` as the separate CAN FD data-frame capability; do not extend
  `CanFrame` beyond eight bytes.
- Keep bxCAN/FDCAN naming and filter/message-RAM allocation private.
- Select `stm32f4/can.cpp`, `stm32h5/can.cpp`, `stm32h7/can.cpp`, or
  `esp32s3/can.cpp` or `posix/can.cpp` at build time; application code never
  selects a CAN engine.
- Put portable bit-timing calculation in a shared helper only if the input clocks
  and constraints can be represented without target leakage.
- Treat receive queues and protocol decoding as higher-level firmware concerns.
- Validate message RAM, filters, pins and peripheral clock before application CAN
  interrupts and project tasks are enabled.
- The STM32H723 FDCAN backend implements `can_fd`; enable it only after the
  target binding's timing, message-RAM, filters and transceiver configuration
  are reviewed.

### ADC

ADC configuration has two levels because several logical sensors commonly share
one physical acquisition engine:

```text
Physical ADC scan engine
├── shared clock, resolution and trigger
├── shared regular-group hardware oversampling
├── one scan sequence, DMA channel and buffer
└── logical sensor channels
    ├── physical input and per-channel sampling time
    ├── independent software-reduction state
    └── public Adc channel handle
```

The application still receives one object per sensor. Sharing an engine does not
turn the sensors into one application object:

```cpp
namespace hardware {

extern drivers::Adc accelerator_primary;
extern drivers::Adc accelerator_secondary;
extern drivers::Adc brake_pedal;

} // namespace hardware
```

The following settings have different ownership:

| Setting | Physical ADC/group | Logical sensor/channel |
| --- | ---: | ---: |
| ADC instance and clock | yes | no |
| Regular-group resolution and hardware oversampling | yes | no |
| Trigger frequency, scan sequence and DMA | yes | no |
| Physical input, pin and supported sampling time | no | yes |
| Software reduction algorithm and window | no | yes |
| Calibration, physical units and plausibility policy | no | portable sensor logic above the driver |

On targets such as STM32H5, regular-group hardware oversampling is a physical ADC
setting and therefore cannot normally be different for two channels converted in
the same running regular scan. This is a silicon resource constraint, not an API
constraint. Different hardware policies require one of:

- placing the sensors on different ADC instances;
- using a compatible regular versus injected acquisition path;
- performing slow on-demand conversions with controlled reconfiguration; or
- selecting one common hardware policy and differentiating the sensors through
  per-channel software reduction.

The last option is preferred for a continuous ECU sensor scan. For example, one
ADC may apply 16-times hardware oversampling with a four-bit shift to every
regular channel while individual sensors use different software windows:

```cpp
binding::AdcScanStorage<4> adc1_scan{
    .device = {
        .instance = binding::AdcInstance::adc1,
        .trigger_frequency_hz = 20'000,
        .hardware_oversampling = {
            .ratio = 16,
            .right_shift = 4, // hardware mean; retains the base full scale
        },
    },
    .channels = {{
        {
            .input = binding::AdcInput::channel1,
            .reduction = {drivers::AdcReduction::moving_mean, 8},
        },
        {
            .input = binding::AdcInput::channel18,
            .reduction = {drivers::AdcReduction::moving_mean, 16},
        },
        {
            .input = binding::AdcInput::channel7,
            .reduction = {drivers::AdcReduction::block_mean, 4},
        },
        {
            .input = binding::AdcInput::channel19,
            .reduction = {drivers::AdcReduction::none, 1},
        },
    }},
};
```

The type and fields above are illustrative backend-private binding APIs. Hardware
oversampling ratio, right shift, register encodings and DMA details do not appear
in application code. If another target lacks equivalent hardware oversampling,
its backend may use repeated conversions and software accumulation to meet the
required acquisition behavior. A target that explicitly requires the hardware
mechanism for a performance or power budget must fail configuration when the
selected device cannot provide it.

Hardware oversampling and software averaging are separate stages:

```text
analog input
    -> physical ADC conversions
    -> optional hardware accumulation and shift
    -> DMA results
    -> per-channel deterministic software reduction
    -> published AdcSample
    -> optional portable sensor filter and unit conversion
```

Use an explicit hardware trigger frequency so that window size has a known time
meaning. Multiplying hardware oversampling and software window sizes also
multiplies the source conversions represented by one output; this must be checked
against ADC throughput, sensor bandwidth and acceptable latency.

`block_mean` consumes a fixed non-overlapping group and publishes once per group.
`moving_mean` maintains a rolling window and may publish whenever new samples are
processed. Do not implement an `average_since_read` policy for control signals:
it makes the filter response depend on task scheduling, makes reading destructive
and allows multiple consumers to interfere.

The backend scan storage owns the DMA buffer and one reduction state per channel.
Half/full-transfer ISR work must be bounded: process only newly completed samples,
use running sums rather than resumming an entire window, and publish a value plus
generation. `latest()` copies that snapshot without resetting it. FreeRTOS task
notification for new data, if useful, belongs in an application adapter above the
driver.

Keep ADC calibration sequences, sampling-time encodings, injected/regular group
rules, DMA selection and register state private to the backend. Keep electrical
calibration, raw-to-voltage conversion, physical units, plausibility checks and
longer-term filters in portable sensor logic. Digital averaging does not replace
the analog anti-alias filter, appropriate source impedance, reference design or
PCB grounding.

### GPIO and PWM

- GPIO pins are bound by the target configuration; application logic sees
  semantic objects.
- PWM can be a timer-channel capability without exposing timer register numbers.

### Hardware timers

A generic public `timer.hpp` is not required and is deliberately omitted from
the interface set. A hardware timer is normally an implementation resource
rather than one application capability:

- PWM uses a timer channel internally;
- ADC sampling may use a timer trigger internally;
- FreeRTOS owns its scheduler tick and task-delay timebase;
- serial, SPI and CAN backends may use a timer or cycle counter for timeouts;
- input capture, pulse counting and encoder mode have different semantics from a
  periodic alarm.

Exposing prescalers, auto-reload registers, channels and compare registers would
leak a target's timer topology into otherwise portable firmware. Backend code
and the target configuration may therefore allocate and program timers without
a public generic timer driver.

A public time-related interface should be added only for a concrete application
requirement, using the capability name rather than `Timer`:

- `monotonic_clock.hpp` for elapsed-time timestamps;
- `alarm.hpp` for one-shot or periodic deadlines outside normal RTOS scheduling;
- `input_capture.hpp` for measuring pulse edges, periods or widths;
- `pulse_counter.hpp` or `quadrature_encoder.hpp` for external event counting.

These interfaces may use STM32 or ESP32-S3 timers internally while remaining
independent of timer numbers and register layouts.

### SPI and serial

- Keep transactions synchronous initially unless asynchronous behavior is
  required and tested.
- Model chip-select ownership explicitly in the target configuration.
- Add DMA internally as an optimization without changing application semantics.
- Specify buffer lifetime and concurrency rules for asynchronous operations.

### DMA

DMA is normally an implementation mechanism, not an application-facing driver.
Keep it private unless application code has a real use case for general memory
transfers. This avoids exposing F4 streams, H5 GPDMA channels, and H7 memory-domain
details in a false common API.

### Storage and RTC

Application persistence should prefer `BlockDevice` or a higher-level storage
interface over SDIO/SDMMC register concepts. Calendar conversion and record
formatting are portable utilities, while the RTC and storage controller access is
target-specific.

`NvMemory` remains a separate byte-addressed contract for calibration and small
persistent records. It may use internal flash and EEPROM emulation without
pretending to be a sector-oriented disk.

### Ethernet and wireless networking

`EthernetMac` is a link-layer driver, not a TCP/IP API. It transfers Ethernet
frames, manages MAC configuration and reports link-layer events to an adapter for
lwIP or another network stack. Application code should normally use the network
stack's socket, UDP or TCP API rather than call `EthernetMac` directly.

On STM32H563VIT6 and STM32H723VGT6, the on-chip Ethernet peripheral is a MAC with
internal queue/DMA machinery. It is not the complete physical Ethernet port. A
working board normally also needs an external 10/100 PHY, an RMII or MII signal
connection supported by the exact package, a PHY reference clock, MDC/MDIO
management signals, PHY reset/interrupt wiring, magnetics and a connector:

```text
application sockets / UDP / TCP
                |
                v
       lwIP or another stack
                |
                v
       network-stack adapter
                |
                v
          EthernetMac API
                |
                v
 static TX/RX buffers + descriptor rings
                |
                v
        STM32 Ethernet DMA/MTL/MAC
                |
             RMII/MII
                |
                v
       external PHY -> magnetics -> cable
```

The target configuration owns the board facts: interface mode, pins, PHY model
and address, reset/interrupt polarity, reference-clock direction/frequency,
unique MAC-address provisioning and DMA memory region. Cable absence is normally
reported as `LinkState::down`, not treated as failed MCU initialization. A
missing/unresponsive PHY or invalid static resource composition is an
initialization error when Ethernet is required by that target.

The STM32 backend owns:

- peripheral clocks/resets and MAC/MTL/DMA register sequencing;
- statically sized, aligned TX/RX descriptor rings and frame buffers;
- MAC address/filter, frame-size and checksum/FCS policy;
- MDIO transactions and the one selected PHY's bounded initialization/link-state
  procedure;
- DMA ownership transitions, barriers, interrupt acknowledgement and diagnostic
  counters;
- direction-correct cache maintenance on cached H5/H7 memory.

PHY code may remain in the selected implementation repository's
`backend/detail/` directory while one target uses one known PHY. It is not another
public driver layer. If several products later require independently reusable PHY
models, introduce a private PHY strategy or a separately reviewed `EthernetPhy`
contract; do not put PHY register names in `ethernet_mac.hpp`.

The initial copying data path is deliberately simple and bounded:

1. `transmit()` validates the caller's frame and tries to claim one free static
   TX slot. With `no_wait`, lack of capacity returns `busy`; a finite timeout has
   an overflow-safe deadline.
2. The backend copies the frame into the slot, cleans every cache line that DMA
   will read on cached targets, publishes the descriptor with the required memory
   barriers and gives ownership to Ethernet DMA.
3. Completion/error interrupt processing acknowledges the source, records status
   and returns the descriptor/slot to the fixed pool. It never allocates or runs
   the network stack.
4. For receive, DMA owns a fixed RX descriptor/buffer until a complete frame is
   written. The ISR records completion and wakes the network task through the
   same fixed callback/context pattern used for CAN.
5. In task context, `receive()` invalidates the completed cache lines, validates
   descriptor errors and length, copies one frame into the caller's span, then
   returns the buffer to DMA with the required barriers. The network-stack
   adapter passes the frame upward.

Ring exhaustion has an explicit policy: TX applies backpressure through
`busy`/`timeout`; RX drops according to a documented deterministic rule and
increments a counter. Descriptors and buffers have static lifetime, DMA-visible
placement and cache-line alignment. Neither the ISR nor steady-state networking
uses project heap allocation. Descriptor ownership is changed only after buffer
and cache operations are complete.

This copying API is appropriate for the first safe implementation and host
contracts. If measured traffic shows that copies are unacceptable, add a
separate zero-copy API with explicit buffer-pool ownership and release operations;
do not reinterpret the lifetime of the existing spans. A FreeRTOS/lwIP adapter
may use task notifications, but the driver backend itself remains FreeRTOS
independent.

The register implementation must be derived independently for each family from
the official [STM32H563/573 documentation page containing RM0481](https://www.st.com/en/microcontrollers-microprocessors/stm32h563-573/documentation.html)
and [STM32H7 RM0468 Ethernet chapter](https://www.st.com/resource/en/reference_manual/dm00603761.pdf),
plus the exact device errata and PHY datasheet. Similar descriptor concepts do
not justify copying undocumented H7 register assumptions into H5.

A POSIX application can use the host socket API above this layer; it does not need
a fake Ethernet MAC unless raw-frame or network-stack integration tests require
one.

The STM32F446VET6 has no integrated Ethernet MAC, so its backend does not
implement `EthernetMac`. A target based on that device must either omit Ethernet
or use an external Ethernet controller through the interface appropriate to that
controller. The build must reject a configuration that requests the on-chip
Ethernet-MAC component for this target.

ESP32-S3 also has no integrated Ethernet MAC. Its Wi-Fi hardware must not be
presented as `EthernetMac`, because Wi-Fi and Ethernet are different link layers.
An ESP32-S3 target may use an external SPI-Ethernet controller when required, but
it implements `EthernetMac` only if that controller exposes the raw-frame
semantics required by the interface.

Portable application networking belongs above the driver layer. A network
service or socket-level adapter can use STM32 Ethernet, ESP32-S3 Wi-Fi or POSIX
sockets without forcing wireless configuration into `ethernet_mac.hpp`. A
dedicated public Wi-Fi capability should be introduced only if application logic
actually needs to control association, credentials or radio state directly.

## Coding and review rules

The codebase uses one predictable vocabulary:

- repositories and release tags use kebab-case;
- directories, files, functions, local variables and enum values use
  `snake_case`;
- C++ types and concepts use `PascalCase`;
- namespaces are `drivers`, `hardware`, `app` and narrow backend-private
  namespaces; there is no company-prefix namespace;
- preprocessor macros are reserved for the build, vendor/core integration and
  FreeRTOS configuration; new project macros use an `ECU_` prefix;
- physical constants include their unit in the name or use a unit-bearing type;
- semantic role names describe purpose (`vehicle_bus`), never an accidental
  instance (`fdcan1`).

Public headers are self-contained and have API contract comments for units,
valid ranges, ownership, blocking, concurrency and errors. Backend comments cite
the reference-manual section or erratum for non-obvious register sequences rather
than restating every line of code. Magic register values, unexplained interrupt
priorities and unbounded loops are rejected in review.

Protocol and persistent formats are serialized field by field with defined byte
order, range and version. Code does not transmit, store or hash the raw memory of
a C++ structure whose padding or representation is implementation-defined.
Unaligned memory is accessed through explicit byte operations or `memcpy`, not a
cast. `volatile` is used for hardware registers and genuinely volatile state, not
as a replacement for atomicity or synchronization.

Formatting is automated. Host static analysis and compiler warnings run in CI;
an embedded-aware rule set can be added incrementally. The project may adopt
MISRA or another safety standard later, but following selected defensive rules
does not justify a compliance claim. Every target change requires review of its
binding/resource plan, safe states, startup behavior, linker map and generated
artifacts, not only a successful compilation.

## Testing and acceptance strategy

Tests are split by what they can actually prove. The `ecu-drivers` repository
keeps the agreed directories:

- `unit/`: host tests for pure code such as CAN timing, deadline arithmetic,
  calendar conversion, reductions, CRCs, ring buffers and storage-record logic;
- `contract/`: the public API exercised against deterministic fakes and the
  POSIX backend, including error paths, invalid ranges, timeout boundaries,
  object state and buffer ownership;
- `compile/`: every public header compiled alone and cross-build/link tests for
  each supported backend, exact device and requested component set;
- backend-local on-target smoke images: small bare-metal executables that bring
  up only one peripheral and its clock/interrupt/DMA dependencies.

The driver tests do not require FreeRTOS. Host register mocks may verify bit-field
composition and state-machine branches, but they do not prove register ordering,
silicon errata, clocking, IRQ delivery, DMA visibility or cache coherency. Those
claims require an on-target test on the exact supported part/module and board
wiring.

The firmware repository owns tests of portable sensor logic and the FreeRTOS
composition. Its POSIX integration build runs the same `app/` modules and verifies:

- ordered initialization and refusal to create tasks after a failed startup
  gate;
- static task/queue creation and correct initial published state;
- CAN ISR-event notification, RX draining and TX queue ownership;
- periodic scheduling without accumulated drift;
- snapshot consistency under concurrent publishers/readers;
- bounded queue-overflow behavior and diagnostic counters;
- heartbeat loss, watchdog non-refresh and fatal-state transition;
- storage and communication fault injection with bounded recovery.

Sanitizers are used in host builds where supported. Tests use a controllable fake
monotonic clock rather than wall-clock sleeps. Random/property tests may augment
edge-case tests for serializers, reductions and timing calculations, but every
reported failure is made reproducible with a recorded seed.

### Continuous-integration gates

Every change must pass:

1. formatting, host warnings-as-errors and static analysis;
2. host unit and contract tests, including an address/undefined-behavior
   sanitizer preset;
3. public-header self-containment checks;
4. a configure, compile and link of every exact supported configuration:
   `stm32f446vet6`, `stm32h563vit6`, `stm32h723vgt6`,
   `esp32-s3-wroom-1-n16r8` and `posix`;
5. negative configuration tests that confirm unsupported components, incompatible
   backend/device pairs and deliberately conflicting resources are rejected;
6. linker-map memory-budget checks and a forbidden-symbol audit for unexpected
   heap, exception, RTTI and static-initialization support in STM32 images;
7. generation and archival of the traceability artifacts defined by the build
   contract.

Hardware-in-the-loop jobs may be scheduled rather than run on every local build,
but a backend release and firmware release cannot be accepted without current
results for the affected exact target. A skipped hardware test is visible and is
not converted into a pass.

### On-target acceptance

The relevant smoke suite verifies, when that component is enabled:

- GPIO safe reset level, input reading and output transition;
- ADC calibration, on-demand conversion, triggered scan order, DMA half/full
  operation, generation updates and configured reduction rate;
- PWM measured frequency, polarity, boundary duties and disabled safe state;
- serial/SPI loopback or a known external fixture, including timeout and error
  recovery;
- CAN internal/external loopback, acceptance filters, bus-off observation and
  recovery policy through the board's actual transceiver;
- watchdog expiry, reset cause and supervisor-only refresh;
- RTC set/read and reset/backup-retention behavior when the target promises it;
- NV memory and block-device alignment, boundary, reset-interruption and recovery
  behavior;
- Ethernet PHY link, frame TX/RX, descriptor exhaustion and DMA/cache visibility
  on targets that expose `EthernetMac`.

An instrumented FreeRTOS image then measures worst-case task execution, interrupt
latency, CPU load, queue high-water marks and stack high-water marks under a
documented stress workload. It deliberately suppresses each required heartbeat
in turn and verifies a watchdog reset and retained diagnostic cause. Long-running
soak duration and traffic/sensor load are defined in the release test plan rather
than hidden in source code.

A target is implementation-complete only when it builds from a clean recursive
checkout, passes host/compile gates, passes smoke tests for every component used
by that target, meets memory/stack/timing budgets, and has a reviewed target
README and artifact manifest. A component that merely compiles remains
experimental and cannot be selected by a release preset.

The POSIX backend is a real selectable implementation for development, but it
does not pretend that every physical capability exists and it is not real-time
evidence. Deterministic fakes remain under `tests/`; they do not become production
backends.

## Inputs required before target implementation

This architecture is sufficient to scaffold the repositories, interfaces, host
tests, CMake selection, sample task modules and POSIX implementation. It does not
invent board facts. Before an embedded target is implemented beyond an isolated
smoke test, its `config/<target>/README.md` and binding plan must be populated
from the authoritative schematic, PCB and product requirements with:

- exact oscillator sources/tolerances, supply range, voltage scaling and required
  CPU/peripheral clocks;
- every semantic signal, package pin, alternate function, active polarity, reset
  pull and actuator safe state;
- external CAN transceiver, Ethernet PHY/controller, RTC, storage and sensor part
  numbers plus enable/reset/interrupt wiring;
- CAN bitrates, identifiers/filters, expected peak traffic and bus-off policy;
- ADC inputs, source impedance, sample rate, sampling time, hardware oversampling,
  per-sensor reduction, calibration and freshness/plausibility limits;
- PWM frequencies, safe duties, required resolution and load behavior;
- serial/SPI modes, rates, transaction bounds and bus-sharing rules;
- watchdog timeout, startup handoff and required task heartbeat periods;
- flash partitions, NV erase/write endurance assumptions, record recovery policy
  and block-device/filesystem ownership;
- interrupt latency, task period/deadline, boot-time, CPU, flash, RAM, stack and
  queue budgets;
- debug/release option bytes, TrustZone, readout protection, secure-boot and
  firmware-update policy.

Unknown values are explicit `TBD` items that prevent the affected release target
from being marked supported; they are not replaced by copied example values. The
illustrative H563 ADC pins, CAN pins and rates in this document are examples of
the binding shape only and must not be used as a board mapping.

Before writing a direct-register peripheral, record the exact reference-manual
revision and current errata items that affect it. Before enabling an actuator,
review its reset-to-initialization interval and verify the electrical safe state
on hardware. These are implementation inputs, not reasons to add more public
driver abstractions.

## Implementation sequence

1. Freeze the public interface contracts from this document and scaffold the
   `ecu-drivers` superproject, backend submodule entries and target-independent
   CMake checks without copying legacy driver internals.
2. Implement `Result`, pure helpers, deterministic fakes and the POSIX backend;
   make unit, contract, sanitizer and negative-configuration tests pass.
3. Scaffold the firmware build, exact target configuration contract and the
   SMU-style sample application using fake/POSIX hardware roles.
4. Make STM32H563VIT6 the first complete embedded vertical slice because an H5
   mapping is already understood by the existing firmware: startup/safe state,
   GPIO, serial diagnostics and watchdog first, then CAN and continuous ADC, then
   only the remaining required components. Every slice includes its bare-metal
   smoke image and FreeRTOS integration test.
5. Implement STM32F446VET6 and STM32H723VGT6 independently from their manuals and
   errata. Reuse portable algorithms, not copied register assumptions. Exercise
   the F4 software ADC reduction and H7 cache/DMA policy explicitly.
6. Add the ESP32-S3 backend as a pinned ESP-IDF component, keep the initial
   `CONFIG_FREERTOS_UNICORE=y` policy and validate the static task helper's byte
   versus word stack-capacity handling.
7. Implement the i.MX 8M Plus M7 vertical slice only after freezing the
   A53/M7 resource partition. Start with M7 boot, memory and GPIO safe states;
   add UART4 and watchdog; then FlexCAN1/2 and PWM; then ECSPI1 plus the
   MCP2518FD. Enable I2C1 and the TLA2024 ADC provider only after exclusive bus
   ownership is proven. Add SNVS RTC only if its retention/ownership contract is
   valid.
8. Migrate real firmware task logic to semantic hardware objects, fixed queues,
   notifications and supervisor heartbeats. Remove physical IDs and X-macro maps
   from application code.
9. Remove old public opaque headers, HAL configuration leakage, family-shaped CAN
   interfaces and unsupported runtime stubs once their replacement vertical
   slices meet the acceptance criteria.
10. Extract a private shared helper only after working backends demonstrate the
   same hardware semantics and tests can remain family-specific.

Each step leaves the tree buildable. A backend or component is not advertised in
a release preset merely because its source directory exists.

## Final decisions

- Use `lib/` as the only top-level container for linked libraries, including
  project-owned and upstream submodules. Do not create a top-level
  `third_party/` until a concrete non-library vendoring need appears.
- Use `backends/`, not a generic top-level `src/`, for selectable implementations.
- Give every implementation repository one private `backend/` source directory,
  no public `include/` directory, and one `<capability>.cpp` per implemented
  public capability. Keep `backend.hpp`, `common.hpp` and optional `detail/`
  private to that backend and the selected target binding.
- Name the superproject repository `ecu-drivers` and the backend repositories
  `ecu-drivers-posix`, `ecu-drivers-esp32s3`, `ecu-drivers-stm32f4`,
  `ecu-drivers-stm32h5`, `ecu-drivers-stm32h7`, and
  `ecu-drivers-imx8mp-m7`.
- Keep `posix`, `stm32f4`, `stm32h5`, `stm32h7`, `esp32s3`, and
  `imx8mp-m7` as independently versioned backend submodules pinned by
  `ecu-drivers`.
- Name backend CMake targets `ecu_drivers_<backend>` and expose the one selected
  firmware composition as `ecu_drivers`.
- Keep public headers directly under `include/`, use includes such as
  `#include <can.hpp>`, and place their C++ symbols in `namespace drivers`; do not
  use `ru`.
- Keep the public architecture strictly interface to selected implementation.
- Start with GPIO, ADC, PWM, serial, SPI, I2C target binding, classic CAN, CAN
  FD, watchdog, RTC, nonvolatile memory, block-device and Ethernet-MAC
  capabilities plus common error types.
- Keep classic CAN and CAN FD as separate public interfaces; bxCAN, FDCAN and
  TWAI are backend details.
- Model continuous ADC acquisition as one configuration-owned physical scan
  engine with separate `Adc` sensor-channel handles. Hardware oversampling
  is group constrained; software reduction may be configured per channel.
- Do not expose a generic `timer.hpp`; add semantic clock, alarm, capture or
  counter interfaces later only when application requirements need them.
- Keep physical mappings in the normalized full-name `config/<target>` directory
  and expose target-defined semantic driver objects to the app.
- Validate exact-device capabilities in CMake and validate the concrete pin,
  peripheral, DMA, timer, interrupt and memory composition in the target binding.
- Do not expose or replicate `opaque_*.hpp`; use private backend state and helpers.
- Give every public driver object one stable non-owning handle; keep all backend
  state, buffers and interrupt bookkeeping in static implementation storage.
- Do not provide fake unsupported implementations.
- Select one configuration in CMake and derive the exact MCU and backend from it.
- Keep each STM32 backend's CMSIS and family-specific vendor dependencies private
  under that backend's `third_party/` directory.
- Use CMSIS device definitions, but not STM32 HAL/LL, for direct-register drivers.
- For i.MX 8M Plus M7, use pinned CMSIS and `MIMX8ML8_cm7` register
  definitions but do not link MCUXpresso `fsl_*` peripheral drivers. Treat
  Linux/A53 ownership, device tree, remoteproc and shared-memory/cache policy as
  part of the exact target contract.
- Build the ESP32-S3 backend as an ESP-IDF component and keep ESP-IDF completely
  private to that backend and target build.
- Start ESP32-S3 with `CONFIG_FREERTOS_UNICORE=y`; treat a future SMP move as an
  architecture and concurrency review.
- Do not model ESP32-S3 Wi-Fi as `EthernetMac`; share portable networking above
  the link-layer drivers.
- Keep the driver library independent of FreeRTOS, but let the sample firmware use
  FreeRTOS directly. Follow the SMU static-task/queue/notification style and do
  not add an RTOS abstraction.
- Use semantic ISR-event connections after task creation, a single-writer data
  model, fixed-capacity queues and supervisor-only watchdog refresh.
- Use C++20/C17, pinned reproducible toolchains, static project-owned embedded
  storage, explicit DMA/cache ownership and no steady-state project allocation.
- Require host/compile tests plus exact-target smoke, FreeRTOS integration,
  timing, memory, stack and fault-injection evidence before release support.
- Share backend code only when hardware behavior is demonstrably the same.
