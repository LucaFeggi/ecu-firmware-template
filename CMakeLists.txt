cmake_minimum_required(VERSION 3.25)

project(ecu_firmware_template VERSION 1.0.0 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ECU_CONFIG posix CACHE STRING "Exact firmware target configuration")
set_property(CACHE ECU_CONFIG PROPERTY STRINGS
    posix stm32f446vet6 stm32h563vit6 stm32h723vgt6 stm32h743vit6
    esp32-s3-wroom-1-n16r8
    verdin-imx8mp-quad-4gb-wb-it-vcu-v9-m7)

set(_valid_configs
    posix stm32f446vet6 stm32h563vit6 stm32h723vgt6 stm32h743vit6
    esp32-s3-wroom-1-n16r8
    verdin-imx8mp-quad-4gb-wb-it-vcu-v9-m7)
if(NOT ECU_CONFIG IN_LIST _valid_configs)
    message(FATAL_ERROR "Unsupported ECU_CONFIG='${ECU_CONFIG}'")
endif()

set(ECU_DRIVERS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/drivers"
    CACHE PATH "Path to the ecu-drivers compatibility repository")
if(NOT EXISTS "${ECU_DRIVERS_DIR}/CMakeLists.txt")
    set(_drivers_sibling "${CMAKE_CURRENT_SOURCE_DIR}/../ecu-drivers")
    if(EXISTS "${_drivers_sibling}/CMakeLists.txt")
        set(ECU_DRIVERS_DIR "${_drivers_sibling}" CACHE PATH "" FORCE)
    else()
        message(FATAL_ERROR
            "ecu-drivers is not initialized. Run the recursive submodule bootstrap "
            "or set ECU_DRIVERS_DIR.")
    endif()
endif()
add_subdirectory("${ECU_DRIVERS_DIR}" ecu-drivers)

function(ecu_enable_project_options target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:
                -Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion;
                -Wshadow;-Wundef;-Werror;-fno-exceptions;-fno-rtti>
            $<$<COMPILE_LANGUAGE:C>:
                -Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion;
                -Wshadow;-Wundef;-Werror>)
    endif()
endfunction()

if(ECU_CONFIG STREQUAL "stm32h743vit6")
    include(config/stm32h743vit6/target.cmake)
    return()
endif()

if(NOT ECU_CONFIG STREQUAL "posix")
    message(FATAL_ERROR
        "The ${ECU_CONFIG} target requires its reviewed board binding values; "
        "see config/${ECU_CONFIG}/README.md")
endif()

set(ECU_DRIVERS_BACKEND_SOURCE_DIR
    "${ECU_DRIVERS_DIR}/backends/posix"
    CACHE PATH "Path to the selected backend repository")
if(NOT EXISTS "${ECU_DRIVERS_BACKEND_SOURCE_DIR}/CMakeLists.txt")
    set(_backend_sibling "${CMAKE_CURRENT_SOURCE_DIR}/../ecu-drivers-posix")
    if(EXISTS "${_backend_sibling}/CMakeLists.txt")
        set(ECU_DRIVERS_BACKEND_SOURCE_DIR
            "${_backend_sibling}" CACHE PATH "" FORCE)
    else()
        message(FATAL_ERROR "The POSIX driver backend is not initialized")
    endif()
endif()

drivers_add_library(ecu_drivers
    BACKEND posix
    DEVICE POSIX
    BACKEND_SOURCE_DIR "${ECU_DRIVERS_BACKEND_SOURCE_DIR}"
    COMPONENTS gpio adc pwm serial can watchdog
    CONFIG_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/config/posix/driver_bindings.cpp")
target_include_directories(ecu_drivers PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/posix")
ecu_enable_project_options(ecu_drivers)

set(FREERTOS_KERNEL_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/freertos"
    CACHE PATH "Path to the pinned FreeRTOS-Kernel checkout")
if(NOT EXISTS "${FREERTOS_KERNEL_DIR}/tasks.c")
    message(FATAL_ERROR
        "FreeRTOS-Kernel is not initialized at '${FREERTOS_KERNEL_DIR}'. "
        "Set FREERTOS_KERNEL_DIR or initialize recursive submodules.")
endif()

set(_freertos_port
    "${FREERTOS_KERNEL_DIR}/portable/ThirdParty/GCC/Posix")
find_package(Threads REQUIRED)

add_library(freertos_kernel STATIC
    "${FREERTOS_KERNEL_DIR}/list.c"
    "${FREERTOS_KERNEL_DIR}/queue.c"
    "${FREERTOS_KERNEL_DIR}/tasks.c"
    "${FREERTOS_KERNEL_DIR}/event_groups.c"
    "${FREERTOS_KERNEL_DIR}/stream_buffer.c"
    "${_freertos_port}/port.c"
    "${_freertos_port}/utils/wait_for_event.c")
target_include_directories(freertos_kernel PUBLIC
    "${FREERTOS_KERNEL_DIR}/include"
    "${_freertos_port}"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/posix")
target_compile_definitions(freertos_kernel PUBLIC
    ECU_CONFIG_POSIX=1)
set_source_files_properties(
    "${_freertos_port}/utils/wait_for_event.c"
    PROPERTIES COMPILE_DEFINITIONS _GNU_SOURCE)
target_link_libraries(freertos_kernel PUBLIC Threads::Threads)

add_library(ecu_application STATIC
    app/app.cpp
    app/tasks/can_rx/can_rx_task.cpp
    app/tasks/can_tx/can_tx_task.cpp
    app/tasks/sensing/sensing_task.cpp
    app/tasks/led/led_task.cpp
    app/tasks/supervisor/supervisor_task.cpp)
target_include_directories(ecu_application PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/app")
target_link_libraries(ecu_application PUBLIC
    ecu_drivers
    freertos_kernel)
ecu_enable_project_options(ecu_application)

add_executable(ecu_firmware
    config/posix/system.cpp
    config/posix/freertos_hooks.cpp)
target_link_libraries(ecu_firmware PRIVATE ecu_application)
ecu_enable_project_options(ecu_firmware)

set(ECU_HOST_TEST_DURATION_TICKS "" CACHE STRING
    "End the POSIX scheduler after this many ticks (integration tests only)")
if(NOT ECU_HOST_TEST_DURATION_TICKS STREQUAL "")
    target_compile_definitions(ecu_application PRIVATE
        ECU_HOST_TEST_DURATION_TICKS=${ECU_HOST_TEST_DURATION_TICKS})
    target_compile_definitions(ecu_firmware PRIVATE
        ECU_HOST_TEST_DURATION_TICKS=${ECU_HOST_TEST_DURATION_TICKS})
endif()

include(CTest)
if(BUILD_TESTING AND NOT ECU_HOST_TEST_DURATION_TICKS STREQUAL "")
    add_test(NAME firmware_posix_boot COMMAND ecu_firmware)
    set_tests_properties(firmware_posix_boot PROPERTIES TIMEOUT 5)
    if(CMAKE_C_FLAGS MATCHES "fsanitize" OR
       CMAKE_CXX_FLAGS MATCHES "fsanitize")
        # The FreeRTOS POSIX port owns process-lifetime pthread resources and
        # does not provide a teardown path. Address/UB checks remain enabled;
        # leak checking is neither meaningful here nor usable under ptrace.
        set_tests_properties(firmware_posix_boot PROPERTIES
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    endif()
endif()
