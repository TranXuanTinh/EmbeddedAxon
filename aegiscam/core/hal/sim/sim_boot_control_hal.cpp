#include "hal/sim/sim_boot_control_hal.h"

#include <cctype>
#include <sstream>

namespace aegis::hal::sim {

namespace {

void skip_ws(std::string_view s, std::size_t& pos) {
  while (pos < s.size() &&
         std::isspace(static_cast<unsigned char>(s[pos])) != 0) {
    ++pos;
  }
}

bool extract_string(std::string_view json, std::string_view key,
                    std::string& out) {
  const std::string needle = "\"" + std::string(key) + "\"";
  std::size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return false;
  }
  pos += needle.size();
  skip_ws(json, pos);
  if (pos >= json.size() || json[pos] != ':') {
    return false;
  }
  ++pos;
  skip_ws(json, pos);
  if (pos >= json.size() || json[pos] != '"') {
    return false;
  }
  ++pos;
  std::string val;
  while (pos < json.size() && json[pos] != '"') {
    if (json[pos] == '\\' && pos + 1 < json.size()) {
      ++pos;
    }
    val.push_back(json[pos]);
    ++pos;
  }
  if (pos >= json.size() || json[pos] != '"') {
    return false;
  }
  out = std::move(val);
  return true;
}

bool extract_u32(std::string_view json, std::string_view key, uint32_t& out) {
  const std::string needle = "\"" + std::string(key) + "\"";
  std::size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return false;
  }
  pos += needle.size();
  skip_ws(json, pos);
  if (pos >= json.size() || json[pos] != ':') {
    return false;
  }
  ++pos;
  skip_ws(json, pos);
  if (pos >= json.size() ||
      std::isdigit(static_cast<unsigned char>(json[pos])) == 0) {
    return false;
  }
  uint32_t val = 0;
  while (pos < json.size() &&
         std::isdigit(static_cast<unsigned char>(json[pos])) != 0) {
    val = val * 10U + static_cast<uint32_t>(json[pos] - '0');
    ++pos;
  }
  out = val;
  return true;
}

bool extract_bool(std::string_view json, std::string_view key, bool& out) {
  const std::string needle = "\"" + std::string(key) + "\"";
  std::size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return false;
  }
  pos += needle.size();
  skip_ws(json, pos);
  if (pos >= json.size() || json[pos] != ':') {
    return false;
  }
  ++pos;
  skip_ws(json, pos);
  if (json.substr(pos, 4) == "true") {
    out = true;
    return true;
  }
  if (json.substr(pos, 5) == "false") {
    out = false;
    return true;
  }
  return false;
}

}  // namespace

SimBootControlHal::SimBootControlHal(IStorageHal& storage, std::string rel_path)
    : storage_(storage), rel_path_(std::move(rel_path)) {}

std::string SimBootControlHal::serialize_state(const BootSlotState& state) {
  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"active_slot\": \"" << state.active_slot << "\",\n";
  oss << "  \"pending_slot\": \"" << state.pending_slot << "\",\n";
  oss << "  \"boot_attempts_left\": " << state.boot_attempts_left << ",\n";
  oss << "  \"marked_good\": " << (state.marked_good ? "true" : "false")
      << ",\n";
  oss << "  \"staged_sha256\": \"" << state.staged_sha256 << "\",\n";
  oss << "  \"staged_image\": \"" << state.staged_image << "\"\n";
  oss << "}\n";
  return oss.str();
}

Result<BootSlotState> SimBootControlHal::parse_state(std::string_view json) {
  BootSlotState s;
  if (!extract_string(json, "active_slot", s.active_slot) ||
      !extract_string(json, "pending_slot", s.pending_slot) ||
      !extract_u32(json, "boot_attempts_left", s.boot_attempts_left) ||
      !extract_bool(json, "marked_good", s.marked_good)) {
    return Result<BootSlotState>::Error(ErrorCode::kParseError,
                                        "Malformed slot_state.json");
  }
  (void)extract_string(json, "staged_sha256", s.staged_sha256);
  (void)extract_string(json, "staged_image", s.staged_image);
  return Result<BootSlotState>::Ok(std::move(s));
}

Result<BootSlotState> SimBootControlHal::load_state() {
  if (!storage_.exists(rel_path_)) {
    return Result<BootSlotState>::Error(ErrorCode::kEmpty,
                                        "slot_state.json does not exist");
  }
  auto text_res = storage_.read_text(rel_path_);
  if (!text_res.ok()) {
    return Result<BootSlotState>(text_res.status());
  }
  return parse_state(text_res.value());
}

Status SimBootControlHal::save_state(const BootSlotState& state) {
  return storage_.write_file_atomic(rel_path_, serialize_state(state));
}

}  // namespace aegis::hal::sim
