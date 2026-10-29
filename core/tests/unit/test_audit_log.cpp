#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "aegis/common/clock.h"
#include "aegis/evidence/audit_log.h"
#include "hal/sim/sim_storage_hal.h"

namespace aegis::evidence {
namespace {

TEST(AuditLogTest, AppendAndVerifyDetectsTampering) {
  const auto temp_dir =
      std::filesystem::temp_directory_path() / "aegis_audit_test";
  std::filesystem::remove_all(temp_dir);

  common::FakeClock clock(1700000000000ULL);
  hal::sim::SimStorageHal storage(temp_dir);
  AuditLog log(storage, clock);
  ASSERT_TRUE(log.init().ok());

  ASSERT_TRUE(log.append("POWER_ON", "device=AEG-0001").ok());
  clock.advance(500);
  ASSERT_TRUE(log.append("INCIDENT_START", "incident=INC-1").ok());
  clock.advance(1000);
  ASSERT_TRUE(log.append("INCIDENT_SEALED", "incident=INC-1").ok());

  EXPECT_EQ(log.last_seq(), 3U);
  EXPECT_TRUE(log.verify().ok());

  // Tamper with audit.log content
  auto raw = storage.read_text("audit.log");
  ASSERT_TRUE(raw.ok());
  std::string tampered = raw.value();
  const auto pos = tampered.find("INC-1");
  ASSERT_NE(pos, std::string::npos);
  tampered[pos + 4] = '9';
  ASSERT_TRUE(storage.write_file_atomic("audit.log", tampered).ok());

  EXPECT_FALSE(log.verify().ok());
  std::filesystem::remove_all(temp_dir);
}

}  // namespace
}  // namespace aegis::evidence
