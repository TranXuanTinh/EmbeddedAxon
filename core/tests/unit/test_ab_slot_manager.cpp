#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "aegis/crypto/sha256.h"
#include "aegis/ota/ab_slot_manager.h"
#include "hal/sim/sim_boot_control_hal.h"
#include "hal/sim/sim_storage_hal.h"

namespace aegis::ota {
namespace {

class AbSlotManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_dir_ = std::filesystem::temp_directory_path() /
                ("aegis_ota_test_" +
                 std::to_string(::testing::UnitTest::GetInstance()
                                    ->random_seed()));
    std::filesystem::remove_all(temp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }

  std::filesystem::path temp_dir_;
};

TEST_F(AbSlotManagerTest, StageRebootAndMarkGoodSwitchesSlot) {
  hal::sim::SimStorageHal storage(temp_dir_);
  hal::sim::SimBootControlHal boot_hal(storage);
  AbSlotManager mgr(boot_hal, storage);
  ASSERT_TRUE(mgr.init().ok());
  EXPECT_EQ(mgr.active_slot(), "A");

  const std::string firmware_payload = "AEGIS-FIRMWARE-V2.0.0-SIGNED";
  ASSERT_TRUE(storage.write_file_atomic("fw_v2.bin", firmware_payload).ok());
  const std::string fw_sha = crypto::Sha256::hash_hex(firmware_payload);

  ASSERT_TRUE(mgr.stage("fw_v2.bin", fw_sha).ok());
  EXPECT_EQ(mgr.pending_slot(), "B");
  EXPECT_EQ(mgr.boot_attempts_left(), 3U);

  // First reboot switches to B and decrements boot_attempts_left -> 2
  ASSERT_TRUE(mgr.on_boot().ok());
  EXPECT_EQ(mgr.active_slot(), "B");
  EXPECT_EQ(mgr.boot_attempts_left(), 2U);
  EXPECT_FALSE(mgr.is_marked_good());

  // Mark good confirms slot B
  ASSERT_TRUE(mgr.mark_good().ok());
  EXPECT_EQ(mgr.active_slot(), "B");
  EXPECT_TRUE(mgr.pending_slot().empty());
  EXPECT_TRUE(mgr.is_marked_good());
}

TEST_F(AbSlotManagerTest, AutomaticRollbackAfterThreeFailedBootAttempts) {
  hal::sim::SimStorageHal storage(temp_dir_);
  hal::sim::SimBootControlHal boot_hal(storage);
  AbSlotManager mgr(boot_hal, storage);
  ASSERT_TRUE(mgr.init().ok());

  const std::string bad_fw = "AEGIS-BROKEN-FIRMWARE";
  ASSERT_TRUE(storage.write_file_atomic("bad.bin", bad_fw).ok());
  const std::string bad_sha = crypto::Sha256::hash_hex(bad_fw);

  ASSERT_TRUE(mgr.stage("bad.bin", bad_sha).ok());

  // Boot 1: active=B, attempts_left=2
  ASSERT_TRUE(mgr.on_boot().ok());
  EXPECT_EQ(mgr.active_slot(), "B");
  EXPECT_EQ(mgr.boot_attempts_left(), 2U);

  // Boot 2: active=B, attempts_left=1
  ASSERT_TRUE(mgr.on_boot().ok());
  EXPECT_EQ(mgr.active_slot(), "B");
  EXPECT_EQ(mgr.boot_attempts_left(), 1U);

  // Boot 3: active=B, attempts_left=0
  ASSERT_TRUE(mgr.on_boot().ok());
  EXPECT_EQ(mgr.active_slot(), "B");
  EXPECT_EQ(mgr.boot_attempts_left(), 0U);

  // Boot 4 without mark_good(): rolls back to slot A!
  ASSERT_TRUE(mgr.on_boot().ok());
  EXPECT_EQ(mgr.active_slot(), "A");
  EXPECT_TRUE(mgr.pending_slot().empty());
  EXPECT_TRUE(mgr.is_marked_good());
}

TEST_F(AbSlotManagerTest, RejectsHashMismatchOnStage) {
  hal::sim::SimStorageHal storage(temp_dir_);
  hal::sim::SimBootControlHal boot_hal(storage);
  AbSlotManager mgr(boot_hal, storage);
  ASSERT_TRUE(mgr.init().ok());

  ASSERT_TRUE(storage.write_file_atomic("fw.bin", "real-bytes").ok());
  const std::string wrong_sha(64, 'a');
  const auto st = mgr.stage("fw.bin", wrong_sha);
  EXPECT_FALSE(st.ok());
  EXPECT_EQ(st.code(), ErrorCode::kIntegrityError);
}

}  // namespace
}  // namespace aegis::ota
