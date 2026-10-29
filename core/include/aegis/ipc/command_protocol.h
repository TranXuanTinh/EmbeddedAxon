#ifndef AEGIS_IPC_COMMAND_PROTOCOL_H_
#define AEGIS_IPC_COMMAND_PROTOCOL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "aegis/common/result.h"

namespace aegis::ipc {

enum class CommandType {
  kTrigger = 0,
  kStop,
  kTick,
  kStatus,
  kVerify,
  kNet,
  kUploadAck,
  kOtaStage,
  kOtaReboot,
  kOtaMarkGood,
  kQuit,
};

struct Command {
  CommandType type{CommandType::kStatus};
  std::string arg1;
  std::string arg2;
  uint64_t numeric_arg{0};
};

struct Response {
  bool ok{true};
  ErrorCode error_code{ErrorCode::kOk};
  std::string payload;  // JSON payload when ok==true, human message when false

  static Response OkJson(std::string json_object) {
    return Response{true, ErrorCode::kOk, std::move(json_object)};
  }

  static Response Error(ErrorCode code, std::string message) {
    return Response{false, code, std::move(message)};
  }
};

class CommandProtocol {
 public:
  static constexpr std::size_t kMaxLineLength = 512;

  // Parses a single line request per spec §5. Never throws or crashes on
  // arbitrary/malformed input.
  static Result<Command> parse_line(std::string_view line) noexcept;

  // Formats a Response as a single protocol line (without trailing '\n').
  static std::string format_response(const Response& response);

  // Helpers for formatting async EVT lines (without trailing '\n').
  static std::string format_segment_sealed_event(std::string_view incident_id,
                                                 uint32_t index,
                                                 std::string_view path,
                                                 std::string_view sha256_hex);

  static std::string format_incident_sealed_event(std::string_view incident_id,
                                                  uint32_t total_segments);
};

}  // namespace aegis::ipc

#endif  // AEGIS_IPC_COMMAND_PROTOCOL_H_
