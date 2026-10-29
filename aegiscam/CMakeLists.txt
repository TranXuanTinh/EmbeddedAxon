cmake_minimum_required(VERSION 3.16)
project(aegiscam VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AEGIS_BUILD_TESTS "Build unit and integration tests" ON)
option(AEGIS_ENABLE_FUZZ "Build libFuzzer targets (Clang only)" OFF)
option(AEGIS_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
option(AEGIS_WERROR "Treat compiler warnings as errors" OFF)
set(AEGIS_SANITIZER "" CACHE STRING "Sanitizer to enable: 'address;undefined', 'thread', or empty")

add_library(aegis_compile_options INTERFACE)
if(MSVC)
  target_compile_options(aegis_compile_options INTERFACE /W4)
  if(AEGIS_WERROR)
    target_compile_options(aegis_compile_options INTERFACE /WX)
  endif()
else()
  target_compile_options(aegis_compile_options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wformat=2
  )
  if(AEGIS_WERROR)
    target_compile_options(aegis_compile_options INTERFACE -Werror)
  endif()
  if(AEGIS_SANITIZER)
    string(REPLACE ";" "," SAN_FLAGS "${AEGIS_SANITIZER}")
    target_compile_options(aegis_compile_options INTERFACE -fsanitize=${SAN_FLAGS} -fno-omit-frame-pointer -g)
    target_link_options(aegis_compile_options INTERFACE -fsanitize=${SAN_FLAGS})
  endif()
  if(AEGIS_ENABLE_COVERAGE)
    target_compile_options(aegis_compile_options INTERFACE --coverage -O0 -g)
    target_link_options(aegis_compile_options INTERFACE --coverage)
  endif()
endif()

find_package(Threads REQUIRED)

add_library(aegis_core STATIC
  core/src/common/logger.cpp
  core/src/crypto/sha256.cpp
  core/src/crypto/hmac.cpp
  core/src/crypto/hash_chain.cpp
  core/src/evidence/pre_event_buffer.cpp
  core/src/evidence/segment_writer.cpp
  core/src/evidence/evidence_store.cpp
  core/src/evidence/audit_log.cpp
  core/src/recording/recording_state_machine.cpp
  core/src/trigger/trigger_arbiter.cpp
  core/src/ota/ab_slot_manager.cpp
  core/src/health/watchdog_supervisor.cpp
  core/src/ipc/command_protocol.cpp
  core/src/app/device_controller.cpp
)

target_include_directories(aegis_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(aegis_core PUBLIC
  aegis_compile_options
  Threads::Threads
)

add_library(aegis_hal_sim STATIC
  core/hal/sim/sim_camera_hal.cpp
  core/hal/sim/sim_storage_hal.cpp
  core/hal/sim/sim_watchdog_hal.cpp
  core/hal/sim/sim_key_store_hal.cpp
  core/hal/sim/sim_boot_control_hal.cpp
)

target_include_directories(aegis_hal_sim PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
)

target_link_libraries(aegis_hal_sim PUBLIC
  aegis_core
  aegis_compile_options
)

if(UNIX AND NOT APPLE)
  add_library(aegis_hal_linux STATIC
    core/hal/linux/linux_watchdog_hal.cpp
    core/hal/linux/posix_storage_hal.cpp
    core/hal/linux/v4l2_camera_hal.cpp
  )
  target_include_directories(aegis_hal_linux PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
  )
  target_link_libraries(aegis_hal_linux PUBLIC
    aegis_core
    aegis_compile_options
  )
endif()

add_executable(aegisd
  core/apps/aegisd_main.cpp
)

target_link_libraries(aegisd PRIVATE
  aegis_core
  aegis_hal_sim
  aegis_compile_options
)

if(UNIX AND NOT APPLE)
  target_link_libraries(aegisd PRIVATE aegis_hal_linux)
endif()

if(AEGIS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(core/tests)
endif()

if(AEGIS_ENABLE_FUZZ)
  add_executable(fuzz_command_protocol
    core/fuzz/fuzz_command_protocol.cpp
  )
  target_compile_options(fuzz_command_protocol PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g)
  target_link_options(fuzz_command_protocol PRIVATE -fsanitize=fuzzer,address,undefined)
  target_link_libraries(fuzz_command_protocol PRIVATE aegis_core)
endif()
