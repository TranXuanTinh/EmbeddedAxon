#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "aegis/app/device_controller.h"
#include "aegis/common/clock.h"
#include "hal/sim/sim_boot_control_hal.h"
#include "hal/sim/sim_camera_hal.h"
#include "hal/sim/sim_key_store_hal.h"
#include "hal/sim/sim_storage_hal.h"
#include "hal/sim/sim_watchdog_hal.h"

namespace aegis::app {
namespace {

TEST(DeviceControllerIntegrationTest,
     EndToEndPreEventTriggerSegmentsVerifyAndShutdown) {
  const auto temp_dir =
      std::filesystem::temp_directory_path() / "aegis_controller_it";
  std::filesystem::remove_all(temp_dir);

  DeviceConfig cfg;
  cfg.device_id = "AEG-0001";
  cfg.pre_event_ms = 5000;
  cfg.segment_ms = 10000;
  cfg.post_record_ms = 5000;
  cfg.fps = 30;
  cfg.sim_mode = true;

  common::FakeClock clock(1700000000000ULL);
  hal::sim::SimCameraHal camera(clock, cfg.fps, 30);
  hal::sim::SimStorageHal storage(temp_dir);
  hal::sim::SimWatchdogHal watchdog;
  hal::sim::SimKeyStoreHal key_store;
  hal::sim::SimBootControlHal boot_control(storage);

  DeviceController controller(cfg, clock, &clock, camera, storage, watchdog,
                              key_store, boot_control);

  std::vector<std::string> events;
  controller.set_event_callback(
      [&](const std::string& evt) { events.push_back(evt); });

  ASSERT_TRUE(controller.start().ok());
  EXPECT_EQ(controller.state(), recording::State::kBuffering);

  // Advance 6 seconds to populate pre-event buffer
  EXPECT_EQ(controller.handle_line("TICK 6000").substr(0, 2), "OK");

  // Trigger holster recording
  EXPECT_EQ(controller.handle_line("TRIGGER holster").substr(0, 2), "OK");
  EXPECT_EQ(controller.state(), recording::State::kRecording);

  // Lower-priority remote STOP must fail with ERR INVALID_STATE
  const std::string bad_stop = controller.handle_line("STOP remote");
  EXPECT_NE(bad_stop.find("ERR INVALID_STATE"), std::string::npos);

  // Advance 20 seconds -> 2 segments cut
  EXPECT_EQ(controller.handle_line("TICK 20000").substr(0, 2), "OK");
  EXPECT_EQ(controller.active_segment_count(), 2U);

  // Stop with holster -> enters PostRecord (5000 ms), then seals after TICK 5000
  EXPECT_EQ(controller.handle_line("STOP holster").substr(0, 2), "OK");
  EXPECT_EQ(controller.state(), recording::State::kPostRecord);

  EXPECT_EQ(controller.handle_line("TICK 5000").substr(0, 2), "OK");
  EXPECT_EQ(controller.state(), recording::State::kBuffering);

  // Verify evidence store and audit log
  const std::string verify_resp = controller.handle_line("VERIFY");
  EXPECT_EQ(verify_resp.substr(0, 2), "OK");
  EXPECT_FALSE(events.empty());

  // Graceful shutdown
  EXPECT_EQ(controller.handle_line("QUIT").substr(0, 2), "OK");
  EXPECT_TRUE(controller.quit_requested());

  std::filesystem::remove_all(temp_dir);
}

}  // namespace
}  // namespace aegis::app
