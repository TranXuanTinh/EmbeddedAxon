#include "aegis/evidence/audit_log.h"

#include <sstream>
#include <vector>

#include "aegis/crypto/sha256.h"

namespace aegis::evidence {

namespace {

std::vector<std::string_view> split_pipe(std::string_view line) {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  while (true) {
    const std::size_t pos = line.find('|', start);
    if (pos == std::string_view::npos) {
      parts.push_back(line.substr(start));
      break;
    }
    parts.push_back(line.substr(start, pos - start));
    start = pos + 1;
  }
  return parts;
}

bool parse_u64(std::string_view s, uint64_t& out) {
  if (s.empty()) {
    return false;
  }
  uint64_t val = 0;
  for (char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
    val = val * 10ULL + static_cast<uint64_t>(c - '0');
  }
  out = val;
  return true;
}

std::string sanitize_field(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '|' || c == '\n' || c == '\r') {
      out.push_back('_');
    } else {
      out.push_back(c);
    }
  }
  return out;
}

}  // namespace

AuditLog::AuditLog(hal::IStorageHal& storage, const common::IClock& clock,
                   std::string rel_path)
    : storage_(storage), clock_(clock), rel_path_(std::move(rel_path)) {}

std::string AuditLog::compute_record_hash(std::string_view prev_hash_hex,
                                          uint64_t seq, uint64_t ts_ms,
                                          std::string_view event,
                                          std::string_view detail) {
  std::string payload;
  payload.reserve(prev_hash_hex.size() + event.size() + detail.size() + 48);
  payload.append(prev_hash_hex);
  payload.push_back('|');
  payload.append(std::to_string(seq));
  payload.push_back('|');
  payload.append(std::to_string(ts_ms));
  payload.push_back('|');
  payload.append(event);
  payload.push_back('|');
  payload.append(detail);
  return crypto::Sha256::hash_hex(payload);
}

Status AuditLog::init() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!storage_.exists(rel_path_)) {
    seq_ = 0;
    prev_hash_hex_ = std::string(kGenesisPrevHash);
    return Status::Ok();
  }
  auto text_res = storage_.read_text(rel_path_);
  if (!text_res.ok()) {
    return text_res.status();
  }

  uint64_t expected_seq = 1;
  std::string expected_prev = std::string(kGenesisPrevHash);
  std::istringstream iss(text_res.value());
  std::string line;
  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    const auto parts = split_pipe(line);
    if (parts.size() != 6) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Malformed audit log line");
    }
    uint64_t seq = 0;
    uint64_t ts_ms = 0;
    if (!parse_u64(parts[0], seq) || !parse_u64(parts[1], ts_ms)) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Invalid numeric field in audit log");
    }
    if (seq != expected_seq) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Audit log sequence discontinuity");
    }
    if (parts[4] != expected_prev) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Audit log prev_hash mismatch");
    }
    const std::string computed =
        compute_record_hash(parts[4], seq, ts_ms, parts[2], parts[3]);
    if (computed != parts[5]) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Audit log record hash mismatch");
    }
    expected_prev = computed;
    ++expected_seq;
  }

  seq_ = expected_seq - 1;
  prev_hash_hex_ = expected_prev;
  return Status::Ok();
}

Status AuditLog::append(std::string_view event, std::string_view detail) {
  std::lock_guard<std::mutex> lock(mutex_);
  const uint64_t next_seq = seq_ + 1;
  const uint64_t ts_ms = clock_.now_ms();
  const std::string clean_event = sanitize_field(event);
  const std::string clean_detail = sanitize_field(detail);
  const std::string hash_hex = compute_record_hash(
      prev_hash_hex_, next_seq, ts_ms, clean_event, clean_detail);

  std::string line = std::to_string(next_seq) + "|" + std::to_string(ts_ms) +
                     "|" + clean_event + "|" + clean_detail + "|" +
                     prev_hash_hex_ + "|" + hash_hex;
  auto st = storage_.append_line(rel_path_, line);
  if (!st.ok()) {
    return st;
  }
  seq_ = next_seq;
  prev_hash_hex_ = hash_hex;
  return Status::Ok();
}

Status AuditLog::verify() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!storage_.exists(rel_path_)) {
    return Status::Ok();
  }
  auto text_res = storage_.read_text(rel_path_);
  if (!text_res.ok()) {
    return Status::Error(ErrorCode::kIntegrityError,
                         "Unable to read audit.log");
  }

  uint64_t expected_seq = 1;
  std::string expected_prev = std::string(kGenesisPrevHash);
  std::istringstream iss(text_res.value());
  std::string line;
  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    const auto parts = split_pipe(line);
    if (parts.size() != 6) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Malformed audit log record");
    }
    uint64_t seq = 0;
    uint64_t ts_ms = 0;
    if (!parse_u64(parts[0], seq) || !parse_u64(parts[1], ts_ms)) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Invalid numeric field in audit log");
    }
    if (seq != expected_seq) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Audit log sequence gap detected");
    }
    if (parts[4] != expected_prev) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Audit log chain broken at seq " +
                               std::to_string(seq));
    }
    const std::string computed =
        compute_record_hash(parts[4], seq, ts_ms, parts[2], parts[3]);
    if (computed != parts[5]) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Audit log hash mismatch at seq " +
                               std::to_string(seq));
    }
    expected_prev = computed;
    ++expected_seq;
  }
  return Status::Ok();
}

uint64_t AuditLog::last_seq() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return seq_;
}

std::string AuditLog::last_hash_hex() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return prev_hash_hex_;
}

}  // namespace aegis::evidence
