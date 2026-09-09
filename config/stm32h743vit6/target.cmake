set(_raceup_freertos_template
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../RaceUP/FreeRTOS_template")
set(ECU_STM32CUBEH7_DIR
    "${_raceup_freertos_template}/third_party/STM32/STM32H7/STM32CubeH7"
    CACHE PATH "Path to STM32CubeH7")
set(FREERTOS_KERNEL_DIR
    "${_raceup_freertos_template}/third_party/FreeRTOS-Kernel"
    CACHE PATH "Path to FreeRTOS-Kernel")
set(ECU_DRIVERS_BACKEND_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../ecu-drivers-stm32h7"
    CACHE PATH "Path to the STM32H7 driver backend")

if(NOT EXISTS "${ECU_STM32CUBEH7_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h743xx.h")
    message(FATAL_ERROR "Set ECU_STM32CUBEH7_DIR to a complete STM32CubeH7 checkout")
endif()
if(NOT EXISTS "${FREERTOS_KERNEL_DIR}/tasks.c")
    message(FATAL_ERROR "Set FREERTOS_KERNEL_DIR to a FreeRTOS-Kernel checkout")
endif()
if(NOT EXISTS "${ECU_DRIVERS_BACKEND_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set ECU_DRIVERS_BACKEND_SOURCE_DIR to ecu-drivers-stm32h7")
endif()

set(ECU_CMSIS_CORE_DIR
    "${ECU_STM32CUBEH7_DIR}/Drivers/CMSIS/Include" CACHE PATH "" FORCE)
set(ECU_CMSIS_DEVICE_H7_DIR
    "${ECU_STM32CUBEH7_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Include"
    CACHE PATH "" FORCE)

drivers_add_library(ecu_drivers
    BACKEND stm32h7
    DEVICE STM32H743xx
    BACKEND_SOURCE_DIR "${ECU_DRIVERS_BACKEND_SOURCE_DIR}"
    COMPONENTS
        gpio adc pwm serial spi i2c can can_fd ethernet_mac block_device
        watchdog rtc nv_memory
    CONFIG_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/config/stm32h743vit6/hardware.cpp")
target_include_directories(ecu_drivers PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/config/stm32h743vit6")
ecu_enable_project_options(ecu_drivers)

add_library(stm32h743_platform INTERFACE)
target_compile_definitions(stm32h743_platform INTERFACE STM32H743xx)
target_compile_options(stm32h743_platform INTERFACE
    -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
    -ffunction-sections -fdata-sections)
target_link_options(stm32h743_platform INTERFACE
    -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard)
target_include_directories(stm32h743_platform SYSTEM INTERFACE
    "${ECU_CMSIS_CORE_DIR}"
    "${ECU_CMSIS_DEVICE_H7_DIR}")
target_link_libraries(ecu_drivers PRIVATE stm32h743_platform)

set(_freertos_port "${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM4F")
add_library(freertos_kernel STATIC
    "${FREERTOS_KERNEL_DIR}/list.c"
    "${FREERTOS_KERNEL_DIR}/queue.c"
    "${FREERTOS_KERNEL_DIR}/tasks.c"
    "${FREERTOS_KERNEL_DIR}/event_groups.c"
    "${FREERTOS_KERNEL_DIR}/stream_buffer.c"
    "${_freertos_port}/port.c")
target_include_directories(freertos_kernel PUBLIC
    "${FREERTOS_KERNEL_DIR}/include"
    "${_freertos_port}"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/stm32h743vit6")
target_link_libraries(freertos_kernel PUBLIC stm32h743_platform)
ecu_enable_project_options(freertos_kernel)

set(_target_dir "${CMAKE_CURRENT_SOURCE_DIR}/config/stm32h743vit6")
add_library(ecu_application STATIC
    "${_target_dir}/app.cpp"
    "${_target_dir}/test_context.cpp"
    "${_target_dir}/tasks/adc/adc_task.cpp"
    "${_target_dir}/tasks/block_device/block_device_task.cpp"
    "${_target_dir}/tasks/can/rx/can_rx_task.cpp"
    "${_target_dir}/tasks/can/tx/can_tx_task.cpp"
    "${_target_dir}/tasks/can_fd/rx/can_fd_rx_task.cpp"
    "${_target_dir}/tasks/can_fd/tx/can_fd_tx_task.cpp"
    "${_target_dir}/tasks/ethernet/rx/ethernet_rx_task.cpp"
    "${_target_dir}/tasks/ethernet/tx/ethernet_tx_task.cpp"
    "${_target_dir}/tasks/gpio/input/gpio_input_task.cpp"
    "${_target_dir}/tasks/gpio/output/gpio_output_task.cpp"
    "${_target_dir}/tasks/i2c/i2c_task.cpp"
    "${_target_dir}/tasks/nv_memory/nv_memory_task.cpp"
    "${_target_dir}/tasks/pwm/pwm_task.cpp"
    "${_target_dir}/tasks/rtc/rtc_task.cpp"
    "${_target_dir}/tasks/serial/rx/serial_rx_task.cpp"
    "${_target_dir}/tasks/serial/tx/serial_tx_task.cpp"
    "${_target_dir}/tasks/spi/spi_task.cpp"
    "${_target_dir}/tasks/watchdog/watchdog_task.cpp")
target_include_directories(ecu_application PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/app"
    "${_target_dir}")
target_link_libraries(ecu_application PUBLIC
    ecu_drivers freertos_kernel stm32h743_platform)
ecu_enable_project_options(ecu_application)

set(_startup
    "${ECU_STM32CUBEH7_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Source/Templates/gcc/startup_stm32h743xx.s")
add_executable(ecu_firmware
    "${_target_dir}/system.cpp"
    "${_target_dir}/freertos_hooks.cpp"
    "${_startup}")
target_link_libraries(ecu_firmware PRIVATE ecu_application stm32h743_platform)
target_link_options(ecu_firmware PRIVATE
    -nostartfiles
    --specs=nano.specs
    --specs=nosys.specs
    "-T${_target_dir}/stm32h743vit6.ld"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/ecu_firmware.map"
    -Wl,--gc-sections
    -Wl,--print-memory-usage)
ecu_enable_project_options(ecu_firmware)
set_target_properties(ecu_firmware PROPERTIES SUFFIX ".elf")

if(CMAKE_OBJCOPY)
    add_custom_command(TARGET ecu_firmware POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" -O ihex
            "$<TARGET_FILE:ecu_firmware>" "${CMAKE_CURRENT_BINARY_DIR}/ecu_firmware.hex"
        COMMAND "${CMAKE_OBJCOPY}" -O binary
            "$<TARGET_FILE:ecu_firmware>" "${CMAKE_CURRENT_BINARY_DIR}/ecu_firmware.bin"
        VERBATIM)
endif()
