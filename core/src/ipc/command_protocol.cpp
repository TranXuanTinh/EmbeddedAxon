#include "aegis/ipc/command_protocol.h"

#include <cctype>
#include <sstream>
#include <vector>

#include "aegis/trigger/trigger_arbiter.h"

namespace aegis::ipc {

namespace {

bool is_printable_ascii(std::string_view s) noexcept {
  for (char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    if (c < 0x20U || c > 0x7EU) {
      return false;
    }
  }
  return true;
}

bool split_single_spaces(std::string_view line,
                         std::vector<std::string_view>& tokens) noexcept {
  if (line.empty()) {
    return false;
  }
  if (line.front() == ' ' || line.back() == ' ') {
    return false;
  }
  std::size_t start = 0;
  while (start < line.size()) {
    const std::size_t pos = line.find(' ', start);
    if (pos == std::string_view::npos) {
      tokens.push_back(line.substr(start));
      break;
    }
    if (pos == start) {
      // Consecutive spaces
      return false;
    }
    tokens.push_back(line.substr(start, pos - start));
    start = pos + 1;
  }
  return !tokens.empty();
}

bool parse_uint64_strict(std::string_view s, uint64_t& out) noexcept {
  if (s.empty() || s.size() > 18) {
    return false;
  }
  uint64_t val = 0;
  for (char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
    val = val * uint64_t{10} + static_cast<uint64_t>(c - '0');
  }
  out = val;
  return true;
}

bool is_valid_hex64(std::string_view s) noexcept {
  if (s.size() != 64) {
    return false;
  }
  for (char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    if (std::isxdigit(c) == 0) {
      return false;
    }
  }
  return true;
}

std::string sanitize_error_message(std::string_view msg) {
  std::string out;
  out.reserve(msg.size());
  for (char c : msg) {
    if (c == '\r' || c == '\n') {
      out.push_back(' ');
    } else {
      out.push_back(c);
    }
  }
  return out;
}

}  // namespace

Result<Command> CommandProtocol::parse_line(std::string_view line) noexcept {
  try {
    if (!line.empty() && line.back() == '\n') {
      line.remove_suffix(1);
    }
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (line.empty()) {
      return Result<Command>::Error(ErrorCode::kParseError, "Empty command");
    }
    if (line.size() > kMaxLineLength) {
      return Result<Command>::Error(ErrorCode::kParseError,
                                    "Command exceeds maximum length of 512");
    }
    if (!is_printable_ascii(line)) {
      return Result<Command>::Error(ErrorCode::kParseError,
                                    "Command contains non-printable ASCII");
    }

    std::vector<std::string_view> tokens;
    tokens.reserve(4);
    if (!split_single_spaces(line, tokens)) {
      return Result<Command>::Error(
          ErrorCode::kParseError,
          "Tokens must be separated by single ASCII spaces");
    }

    const std::string_view verb = tokens[0];

    if (verb == "TRIGGER") {
      if (tokens.size() != 2) {
        return Result<Command>::Error(ErrorCode::kParseError,
                                      "Usage: TRIGGER <src>");
      }
      auto src_res = trigger::parse_trigger_source(tokens[1]);
      if (!src_res.ok()) {
        return Result<Command>::Error(ErrorCode::kInvalidArgument,
                                      src_res.message());
      }
      Command cmd;
      cmd.type = CommandType::kTrigger;
      cmd.arg1 = std::string(tokens[1]);
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "STOP") {
      if (tokens.size() != 2) {
        return Result<Command>::Error(ErrorCode::kParseError,
                                      "Usage: STOP <src>");
      }
      auto src_res = trigger::parse_trigger_source(tokens[1]);
      if (!src_res.ok()) {
        return Result<Command>::Error(ErrorCode::kInvalidArgument,
                                      src_res.message());
      }
      Command cmd;
      cmd.type = CommandType::kStop;
      cmd.arg1 = std::string(tokens[1]);
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "TICK") {
      if (tokens.size() != 2) {
        return Result<Command>::Error(ErrorCode::kParseError,
                                      "Usage: TICK <ms>");
      }
      uint64_t ms = 0;
      if (!parse_uint64_strict(tokens[1], ms) || ms < 1 || ms > 60000) {
        return Result<Command>::Error(ErrorCode::kInvalidArgument,
                                      "TICK ms must be in range 1..60000");
      }
      Command cmd;
      cmd.type = CommandType::kTick;
      cmd.numeric_arg = ms;
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "STATUS") {
      if (tokens.size() != 1) {
        return Result<Command>::Error(ErrorCode::kParseError,
                                      "STATUS takes no arguments");
      }
      Command cmd;
      cmd.type = CommandType::kStatus;
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "VERIFY") {
      if (tokens.size() != 1) {
        return Result<Command>::Error(ErrorCode::kParseError,
                                      "VERIFY takes no arguments");
      }
      Command cmd;
      cmd.type = CommandType::kVerify;
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "NET") {
      if (tokens.size() != 2) {
        return Result<Command>::Error(ErrorCode::kParseError,
                                      "Usage: NET <up|down>");
      }
      if (tokens[1] != "up" && tokens[1] != "down") {
        return Result<Command>::Error(ErrorCode::kInvalidArgument,
                                      "NET state must be 'up' or 'down'");
      }
      Command cmd;
      cmd.type = CommandType::kNet;
      cmd.arg1 = std::string(tokens[1]);
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "UPLOAD_ACK") {
      if (tokens.size() != 3) {
        return Result<Command>::Error(
            ErrorCode::kParseError,
            "Usage: UPLOAD_ACK <incident_id> <segment_index>");
      }
      uint64_t seg_idx = 0;
      if (!parse_uint64_strict(tokens[2], seg_idx) || seg_idx < 1 ||
          seg_idx > 1000000ULL) {
        return Result<Command>::Error(ErrorCode::kInvalidArgument,
                                      "Invalid segment_index");
      }
      Command cmd;
      cmd.type = CommandType::kUploadAck;
      cmd.arg1 = std::string(tokens[1]);
      cmd.numeric_arg = seg_idx;
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "OTA_STAGE") {
      if (tokens.size() != 3) {
        return Result<Command>::Error(
            ErrorCode::kParseError, "Usage: OTA_STAGE <image_path> <sha256hex>");
      }
      if (!is_valid_hex64(tokens[2])) {
        return Result<Command>::Error(ErrorCode::kInvalidArgument,
                                      "sha256hex must be 64 hex characters");
      }
      Command cmd;
      cmd.type = CommandType::kOtaStage;
      cmd.arg1 = std::string(tokens[1]);
      cmd.arg2 = std::string(tokens[2]);
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "OTA_REBOOT") {
      if (tokens.size() != 1) {
        return Result<Command>::Error(ErrorCode::kParseError,
                                      "OTA_REBOOT takes no arguments");
      }
      Command cmd;
      cmd.type = CommandType::kOtaReboot;
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "OTA_MARK_GOOD") {
      if (tokens.size() != 1) {
        return Result<Command>::Error(ErrorCode::kParseError,
                                      "OTA_MARK_GOOD takes no arguments");
      }
      Command cmd;
      cmd.type = CommandType::kOtaMarkGood;
      return Result<Command>::Ok(std::move(cmd));
    }

    if (verb == "QUIT") {
      if (tokens.size() != 1) {
        return Result<Command>::Error(ErrorCode::kParseError,
                                      "QUIT takes no arguments");
      }
      Command cmd;
      cmd.type = CommandType::kQuit;
      return Result<Command>::Ok(std::move(cmd));
    }

    return Result<Command>::Error(ErrorCode::kParseError,
                                  "Unknown command verb: " + std::string(verb));
  } catch (...) {
    return Result<Command>::Error(ErrorCode::kParseError,
                                  "Unhandled parse exception");
  }
}

std::string CommandProtocol::format_response(const Response& response) {
  if (response.ok) {
    return "OK " + response.payload;
  }
  return std::string("ERR ") + error_code_to_string(response.error_code) + " " +
         sanitize_error_message(response.payload);
}

std::string CommandProtocol::format_segment_sealed_event(
    std::string_view incident_id, uint32_t index, std::string_view path,
    std::string_view sha256_hex) {
  std::ostringstream oss;
  oss << "EVT {\"type\":\"segment_sealed\",\"incident\":\"" << incident_id
      << "\",\"index\":" << index << ",\"path\":\"" << path
      << "\",\"sha256\":\"" << sha256_hex << "\"}";
  return oss.str();
}

std::string CommandProtocol::format_incident_sealed_event(
    std::string_view incident_id, uint32_t total_segments) {
  std::ostringstream oss;
  oss << "EVT {\"type\":\"incident_sealed\",\"incident\":\"" << incident_id
      << "\",\"segments\":" << total_segments << "}";
  return oss.str();
}

}  // namespace aegis::ipc
