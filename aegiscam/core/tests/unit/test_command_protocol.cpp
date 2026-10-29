#include <gtest/gtest.h>

#include <string>

#include "aegis/ipc/command_protocol.h"

namespace aegis::ipc {
namespace {

TEST(CommandProtocolTest, ParsesValidCommands) {
  auto r1 = CommandProtocol::parse_line("TRIGGER holster\n");
  ASSERT_TRUE(r1.ok());
  EXPECT_EQ(r1.value().type, CommandType::kTrigger);
  EXPECT_EQ(r1.value().arg1, "holster");

  auto r2 = CommandProtocol::parse_line("TICK 10000");
  ASSERT_TRUE(r2.ok());
  EXPECT_EQ(r2.value().type, CommandType::kTick);
  EXPECT_EQ(r2.value().numeric_arg, 10000U);

  auto r3 = CommandProtocol::parse_line("UPLOAD_ACK INC-AEG-0001-100 3");
  ASSERT_TRUE(r3.ok());
  EXPECT_EQ(r3.value().type, CommandType::kUploadAck);
  EXPECT_EQ(r3.value().arg1, "INC-AEG-0001-100");
  EXPECT_EQ(r3.value().numeric_arg, 3U);

  const std::string sha(64, '0');
  auto r4 = CommandProtocol::parse_line("OTA_STAGE /tmp/fw.bin " + sha);
  ASSERT_TRUE(r4.ok());
  EXPECT_EQ(r4.value().type, CommandType::kOtaStage);
}

TEST(CommandProtocolTest, RejectsMalformedAndOutOfRangeInputs) {
  EXPECT_FALSE(CommandProtocol::parse_line("").ok());
  EXPECT_FALSE(CommandProtocol::parse_line("   ").ok());
  EXPECT_FALSE(CommandProtocol::parse_line("TRIGGER  holster").ok());
  EXPECT_FALSE(CommandProtocol::parse_line("TRIGGER unknown_src").ok());
  EXPECT_FALSE(CommandProtocol::parse_line("TICK 0").ok());
  EXPECT_FALSE(CommandProtocol::parse_line("TICK 60001").ok());
  EXPECT_FALSE(CommandProtocol::parse_line("NET maybe").ok());

  const std::string too_long(600, 'A');
  EXPECT_FALSE(CommandProtocol::parse_line(too_long).ok());
}

}  // namespace
}  // namespace aegis::ipc
