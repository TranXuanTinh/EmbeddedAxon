include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(aegis_tests
  unit/test_sha256.cpp
  unit/test_hmac.cpp
  unit/test_hash_chain.cpp
  unit/test_spsc_ring_buffer.cpp
  unit/test_bounded_queue.cpp
  unit/test_pre_event_buffer.cpp
  unit/test_recording_state_machine.cpp
  unit/test_trigger_arbiter.cpp
  unit/test_ab_slot_manager.cpp
  unit/test_audit_log.cpp
  unit/test_command_protocol.cpp
  integration/test_device_controller.cpp
)

target_link_libraries(aegis_tests PRIVATE
  aegis_core
  aegis_hal_sim
  GTest::gtest_main
  aegis_compile_options
)

gtest_discover_tests(aegis_tests)
