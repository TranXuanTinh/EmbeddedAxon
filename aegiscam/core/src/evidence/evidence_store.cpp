#include "aegis/evidence/evidence_store.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "aegis/crypto/hmac.h"
#include "aegis/crypto/sha256.h"

namespace aegis::evidence {

namespace {

std::string json_escape(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

void skip_ws(std::string_view s, std::size_t& pos) {
  while (pos < s.size() &&
         std::isspace(static_cast<unsigned char>(s[pos])) != 0) {
    ++pos;
  }
}

bool extract_string_field(std::string_view json, std::string_view key,
                          std::string& out, std::size_t start_pos = 0) {
  const std::string needle = "\"" + std::string(key) + "\"";
  std::size_t pos = json.find(needle, start_pos);
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
  std::string result;
  while (pos < json.size() && json[pos] != '"') {
    if (json[pos] == '\\' && pos + 1 < json.size()) {
      ++pos;
    }
    result.push_back(json[pos]);
    ++pos;
  }
  if (pos >= json.size() || json[pos] != '"') {
    return false;
  }
  out = std::move(result);
  return true;
}

bool extract_uint64_field(std::string_view json, std::string_view key,
                          uint64_t& out, std::size_t start_pos = 0) {
  const std::string needle = "\"" + std::string(key) + "\"";
  std::size_t pos = json.find(needle, start_pos);
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
  uint64_t val = 0;
  while (pos < json.size() &&
         std::isdigit(static_cast<unsigned char>(json[pos])) != 0) {
    val = val * 10ULL + static_cast<uint64_t>(json[pos] - '0');
    ++pos;
  }
  out = val;
  return true;
}

bool extract_bool_field(std::string_view json, std::string_view key, bool& out,
                        std::size_t start_pos = 0) {
  const std::string needle = "\"" + std::string(key) + "\"";
  std::size_t pos = json.find(needle, start_pos);
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

EvidenceStore::EvidenceStore(hal::IStorageHal& storage,
                             hal::IKeyStoreHal& key_store,
                             std::string device_id, uint64_t pre_event_ms)
    : storage_(storage),
      key_store_(key_store),
      device_id_(std::move(device_id)),
      pre_event_ms_(pre_event_ms),
      writer_(storage) {}

Result<std::string> EvidenceStore::begin_incident(
    uint64_t start_ts_ms, std::string_view trigger_source) {
  const std::string incident_id =
      "INC-" + device_id_ + "-" + std::to_string(start_ts_ms);
  const std::string incident_dir = "incidents/" + incident_id;
  auto mkdir_st = storage_.mkdir_p(incident_dir);
  if (!mkdir_st.ok()) {
    return Result<std::string>(mkdir_st);
  }

  auto key_res = key_store_.get_device_hmac_key();
  if (!key_res.ok()) {
    return Result<std::string>(key_res.status());
  }

  ActiveIncident active;
  active.manifest.version = 1;
  active.manifest.device_id = device_id_;
  active.manifest.incident_id = incident_id;
  active.manifest.trigger_source = std::string(trigger_source);
  active.manifest.pre_event_ms = pre_event_ms_;
  active.manifest.chain_head = active.chain.head_hex();
  active.manifest.hmac = crypto::HmacSha256::compute_hex(
      key_res.value(), active.manifest.chain_head);
  active.manifest.sealed = false;

  auto st = persist_manifest(active.manifest);
  if (!st.ok()) {
    return Result<std::string>(st);
  }

  incidents_[incident_id] = std::move(active);
  return Result<std::string>::Ok(incident_id);
}

Result<ManifestSegmentEntry> EvidenceStore::append_segment(
    std::string_view incident_id, const std::vector<Frame>& frames) {
  auto it = incidents_.find(std::string(incident_id));
  if (it == incidents_.end()) {
    return Result<ManifestSegmentEntry>::Error(ErrorCode::kInvalidState,
                                               "Unknown active incident");
  }
  auto& active = it->second;
  if (active.manifest.sealed) {
    return Result<ManifestSegmentEntry>::Error(ErrorCode::kInvalidState,
                                               "Incident already sealed");
  }

  const uint32_t next_index =
      static_cast<uint32_t>(active.manifest.segments.size() + 1);
  auto seg_res = writer_.write_segment(incident_id, next_index, frames);
  if (!seg_res.ok()) {
    return Result<ManifestSegmentEntry>(seg_res.status());
  }

  auto key_res = key_store_.get_device_hmac_key();
  if (!key_res.ok()) {
    return Result<ManifestSegmentEntry>(key_res.status());
  }

  const auto& seg_info = seg_res.value();
  const auto chain_digest = active.chain.advance(seg_info.sha256);
  const std::string chain_hex = crypto::Sha256::to_hex(chain_digest);

  ManifestSegmentEntry entry;
  entry.index = seg_info.index;
  entry.file = seg_info.file;
  entry.bytes = seg_info.bytes;
  entry.sha256 = seg_info.sha256_hex;
  entry.chain = chain_hex;

  active.manifest.segments.push_back(entry);
  active.manifest.chain_head = chain_hex;
  active.manifest.hmac = crypto::HmacSha256::compute_hex(
      key_res.value(), active.manifest.chain_head);

  auto st = persist_manifest(active.manifest);
  if (!st.ok()) {
    return Result<ManifestSegmentEntry>(st);
  }
  return Result<ManifestSegmentEntry>::Ok(entry);
}

Status EvidenceStore::seal_incident(std::string_view incident_id) {
  auto it = incidents_.find(std::string(incident_id));
  if (it == incidents_.end()) {
    return Status::Error(ErrorCode::kInvalidState, "Unknown incident");
  }
  auto& active = it->second;
  if (active.manifest.sealed) {
    return Status::Ok();
  }
  auto key_res = key_store_.get_device_hmac_key();
  if (!key_res.ok()) {
    return key_res.status();
  }
  active.manifest.sealed = true;
  active.manifest.hmac = crypto::HmacSha256::compute_hex(
      key_res.value(), active.manifest.chain_head);
  return persist_manifest(active.manifest);
}

Status EvidenceStore::mark_segment_uploaded(std::string_view incident_id,
                                            uint32_t segment_index) {
  auto it = incidents_.find(std::string(incident_id));
  if (it == incidents_.end()) {
    return Status::Error(ErrorCode::kInvalidArgument, "Unknown incident_id");
  }
  for (auto& seg : it->second.manifest.segments) {
    if (seg.index == segment_index) {
      seg.uploaded = true;
      return Status::Ok();
    }
  }
  return Status::Error(ErrorCode::kInvalidArgument, "Unknown segment_index");
}

Status EvidenceStore::persist_manifest(const IncidentManifest& manifest) {
  const std::string rel_path =
      "incidents/" + manifest.incident_id + "/manifest.json";
  const std::string json = serialize_manifest(manifest);
  return storage_.write_file_atomic(rel_path, json);
}

Result<IncidentManifest> EvidenceStore::get_manifest(
    std::string_view incident_id) const {
  auto it = incidents_.find(std::string(incident_id));
  if (it != incidents_.end()) {
    return Result<IncidentManifest>::Ok(it->second.manifest);
  }
  const std::string rel_path =
      "incidents/" + std::string(incident_id) + "/manifest.json";
  auto text_res = storage_.read_text(rel_path);
  if (!text_res.ok()) {
    return Result<IncidentManifest>(text_res.status());
  }
  return parse_manifest(text_res.value());
}

std::string EvidenceStore::serialize_manifest(
    const IncidentManifest& manifest) {
  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"version\": " << manifest.version << ",\n";
  oss << "  \"device_id\": \"" << json_escape(manifest.device_id) << "\",\n";
  oss << "  \"incident_id\": \"" << json_escape(manifest.incident_id)
      << "\",\n";
  oss << "  \"trigger_source\": \"" << json_escape(manifest.trigger_source)
      << "\",\n";
  oss << "  \"pre_event_ms\": " << manifest.pre_event_ms << ",\n";
  oss << "  \"segments\": [";
  if (!manifest.segments.empty()) {
    oss << "\n";
    for (std::size_t i = 0; i < manifest.segments.size(); ++i) {
      const auto& s = manifest.segments[i];
      oss << "    {\"index\": " << s.index << ", \"file\": \""
          << json_escape(s.file) << "\", \"bytes\": " << s.bytes
          << ", \"sha256\": \"" << json_escape(s.sha256) << "\", \"chain\": \""
          << json_escape(s.chain) << "\"}";
      if (i + 1 < manifest.segments.size()) {
        oss << ",";
      }
      oss << "\n";
    }
    oss << "  ";
  }
  oss << "],\n";
  oss << "  \"chain_head\": \"" << json_escape(manifest.chain_head) << "\",\n";
  oss << "  \"hmac\": \"" << json_escape(manifest.hmac) << "\",\n";
  oss << "  \"sealed\": " << (manifest.sealed ? "true" : "false") << "\n";
  oss << "}\n";
  return oss.str();
}

Result<IncidentManifest> EvidenceStore::parse_manifest(std::string_view json) {
  IncidentManifest m;
  uint64_t version = 0;
  if (!extract_uint64_field(json, "version", version)) {
    return Result<IncidentManifest>::Error(ErrorCode::kParseError,
                                           "Missing version in manifest");
  }
  m.version = static_cast<uint32_t>(version);
  if (!extract_string_field(json, "device_id", m.device_id) ||
      !extract_string_field(json, "incident_id", m.incident_id) ||
      !extract_string_field(json, "trigger_source", m.trigger_source) ||
      !extract_uint64_field(json, "pre_event_ms", m.pre_event_ms) ||
      !extract_string_field(json, "chain_head", m.chain_head) ||
      !extract_string_field(json, "hmac", m.hmac) ||
      !extract_bool_field(json, "sealed", m.sealed)) {
    return Result<IncidentManifest>::Error(
        ErrorCode::kParseError, "Missing required fields in manifest.json");
  }

  const std::size_t seg_key = json.find("\"segments\"");
  if (seg_key == std::string_view::npos) {
    return Result<IncidentManifest>::Error(ErrorCode::kParseError,
                                           "Missing segments array");
  }
  const std::size_t arr_start = json.find('[', seg_key);
  const std::size_t arr_end = json.find(']', arr_start);
  if (arr_start == std::string_view::npos ||
      arr_end == std::string_view::npos) {
    return Result<IncidentManifest>::Error(ErrorCode::kParseError,
                                           "Malformed segments array");
  }

  std::string_view seg_slice = json.substr(arr_start + 1, arr_end - arr_start - 1);
  std::size_t pos = 0;
  while (true) {
    const std::size_t obj_start = seg_slice.find('{', pos);
    if (obj_start == std::string_view::npos) {
      break;
    }
    const std::size_t obj_end = seg_slice.find('}', obj_start);
    if (obj_end == std::string_view::npos) {
      return Result<IncidentManifest>::Error(ErrorCode::kParseError,
                                             "Unclosed segment object");
    }
    std::string_view obj = seg_slice.substr(obj_start, obj_end - obj_start + 1);
    ManifestSegmentEntry entry;
    uint64_t idx = 0;
    uint64_t bytes = 0;
    if (!extract_uint64_field(obj, "index", idx) ||
        !extract_string_field(obj, "file", entry.file) ||
        !extract_uint64_field(obj, "bytes", bytes) ||
        !extract_string_field(obj, "sha256", entry.sha256) ||
        !extract_string_field(obj, "chain", entry.chain)) {
      return Result<IncidentManifest>::Error(ErrorCode::kParseError,
                                             "Malformed segment entry");
    }
    entry.index = static_cast<uint32_t>(idx);
    entry.bytes = static_cast<std::size_t>(bytes);
    m.segments.push_back(std::move(entry));
    pos = obj_end + 1;
  }

  return Result<IncidentManifest>::Ok(std::move(m));
}

Status EvidenceStore::verify_incident(std::string_view incident_id) const {
  const std::string incident_dir = "incidents/" + std::string(incident_id);
  const std::string manifest_path = incident_dir + "/manifest.json";
  auto text_res = storage_.read_text(manifest_path);
  if (!text_res.ok()) {
    return Status::Error(ErrorCode::kIntegrityError,
                         "Cannot read manifest.json for " +
                             std::string(incident_id));
  }
  auto manifest_res = parse_manifest(text_res.value());
  if (!manifest_res.ok()) {
    return Status::Error(ErrorCode::kIntegrityError,
                         "Corrupt manifest.json: " + manifest_res.message());
  }
  const auto& manifest = manifest_res.value();

  if (manifest.incident_id != incident_id) {
    return Status::Error(ErrorCode::kIntegrityError,
                         "Manifest incident_id does not match directory");
  }

  crypto::HashChain chain;
  for (std::size_t i = 0; i < manifest.segments.size(); ++i) {
    const auto& seg = manifest.segments[i];
    if (seg.index != i + 1) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Segment index gap or disorder in manifest");
    }
    const std::string seg_path = incident_dir + "/" + seg.file;
    auto file_res = storage_.read_file(seg_path);
    if (!file_res.ok()) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Missing segment file: " + seg.file);
    }
    const auto& data = file_res.value();
    if (data.size() != seg.bytes) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Segment byte count mismatch: " + seg.file);
    }
    const auto actual_digest = crypto::Sha256::hash(data);
    const std::string actual_hex = crypto::Sha256::to_hex(actual_digest);
    if (actual_hex != seg.sha256) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Segment SHA-256 mismatch: " + seg.file);
    }
    const auto expected_chain = chain.advance(actual_digest);
    if (crypto::Sha256::to_hex(expected_chain) != seg.chain) {
      return Status::Error(ErrorCode::kIntegrityError,
                           "Segment hash chain mismatch: " + seg.file);
    }
  }

  auto files_res = storage_.list_files(incident_dir);
  if (files_res.ok()) {
    for (const auto& fname : files_res.value()) {
      if (fname.rfind("seg_", 0) == 0 && fname.size() > 4 &&
          fname.substr(fname.size() - 4) == ".bin") {
        bool found = false;
        for (const auto& s : manifest.segments) {
          if (s.file == fname) {
            found = true;
            break;
          }
        }
        if (!found) {
          return Status::Error(ErrorCode::kIntegrityError,
                               "Unreferenced segment file on disk: " + fname);
        }
      }
    }
  }

  if (manifest.chain_head != chain.head_hex()) {
    return Status::Error(ErrorCode::kIntegrityError,
                         "Manifest chain_head mismatch");
  }

  auto key_res = key_store_.get_device_hmac_key();
  if (!key_res.ok()) {
    return key_res.status();
  }
  const auto expected_hmac =
      crypto::HmacSha256::compute(key_res.value(), manifest.chain_head);
  auto actual_hmac_res = crypto::Sha256::from_hex(manifest.hmac);
  if (!actual_hmac_res.ok() ||
      !crypto::HmacSha256::verify_constant_time(expected_hmac,
                                                actual_hmac_res.value())) {
    return Status::Error(ErrorCode::kIntegrityError,
                         "Manifest HMAC verification failed");
  }

  return Status::Ok();
}

Status EvidenceStore::verify_all() const {
  if (!storage_.exists("incidents")) {
    return Status::Ok();
  }
  auto dirs_res = storage_.list_dirs("incidents");
  if (!dirs_res.ok()) {
    return dirs_res.status();
  }
  auto dirs = dirs_res.value();
  std::sort(dirs.begin(), dirs.end());
  for (const auto& dir : dirs) {
    auto st = verify_incident(dir);
    if (!st.ok()) {
      return st;
    }
  }
  return Status::Ok();
}

Status EvidenceStore::recover() {
  if (!storage_.exists("incidents")) {
    return Status::Ok();
  }
  auto dirs_res = storage_.list_dirs("incidents");
  if (!dirs_res.ok()) {
    return dirs_res.status();
  }
  for (const auto& inc_id : dirs_res.value()) {
    const std::string inc_dir = "incidents/" + inc_id;
    auto files_res = storage_.list_files(inc_dir);
    if (files_res.ok()) {
      for (const auto& fname : files_res.value()) {
        if (fname.find(".tmp.") != std::string::npos) {
          (void)storage_.remove_file(inc_dir + "/" + fname);
        }
      }
    }
    const std::string manifest_path = inc_dir + "/manifest.json";
    if (!storage_.exists(manifest_path)) {
      continue;
    }
    auto text_res = storage_.read_text(manifest_path);
    if (!text_res.ok()) {
      continue;
    }
    auto manifest_res = parse_manifest(text_res.value());
    if (!manifest_res.ok()) {
      continue;
    }
    auto manifest = manifest_res.value();
    if (!manifest.sealed) {
      manifest.sealed = true;
      (void)persist_manifest(manifest);
    }
  }
  return Status::Ok();
}

}  // namespace aegis::evidence
