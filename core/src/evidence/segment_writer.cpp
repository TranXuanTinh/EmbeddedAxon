#include "aegis/evidence/segment_writer.h"

#include <cstdio>

namespace aegis::evidence {

namespace {

void append_u64_le(std::vector<uint8_t>& out, uint64_t val) {
  for (int i = 0; i < 8; ++i) {
    out.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFFU));
  }
}

void append_u32_le(std::vector<uint8_t>& out, uint32_t val) {
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFFU));
  }
}

}  // namespace

SegmentWriter::SegmentWriter(hal::IStorageHal& storage) noexcept
    : storage_(storage) {}

std::string SegmentWriter::format_segment_filename(uint32_t segment_index) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "seg_%06u.bin", segment_index);
  return std::string(buf);
}

std::vector<uint8_t> SegmentWriter::serialize_frames(
    const std::vector<Frame>& frames) {
  std::vector<uint8_t> buffer;
  // Header magic "AEGS" + version 1 + frame count
  buffer.push_back('A');
  buffer.push_back('E');
  buffer.push_back('G');
  buffer.push_back('S');
  append_u32_le(buffer, static_cast<uint32_t>(frames.size()));

  for (const auto& f : frames) {
    append_u64_le(buffer, f.seq);
    append_u64_le(buffer, f.ts_ms);
    buffer.push_back(f.keyframe ? 1U : 0U);
    append_u32_le(buffer, static_cast<uint32_t>(f.payload.size()));
    buffer.insert(buffer.end(), f.payload.begin(), f.payload.end());
  }
  return buffer;
}

Result<SegmentInfo> SegmentWriter::write_segment(
    std::string_view incident_id, uint32_t segment_index,
    const std::vector<Frame>& frames) {
  const auto bytes = serialize_frames(frames);
  return write_raw_segment(incident_id, segment_index, bytes.data(),
                           bytes.size());
}

Result<SegmentInfo> SegmentWriter::write_raw_segment(
    std::string_view incident_id, uint32_t segment_index, const uint8_t* data,
    std::size_t len) {
  if (segment_index == 0) {
    return Result<SegmentInfo>::Error(ErrorCode::kInvalidArgument,
                                      "Segment index must be >= 1");
  }
  const std::string filename = format_segment_filename(segment_index);
  const std::string incident_dir = "incidents/" + std::string(incident_id);
  auto mkdir_st = storage_.mkdir_p(incident_dir);
  if (!mkdir_st.ok()) {
    return Result<SegmentInfo>(mkdir_st);
  }

  const std::string rel_path = incident_dir + "/" + filename;
  auto write_st = storage_.write_file_atomic(rel_path, data, len);
  if (!write_st.ok()) {
    return Result<SegmentInfo>(write_st);
  }

  const auto digest = crypto::Sha256::hash(data, len);
  SegmentInfo info;
  info.index = segment_index;
  info.file = filename;
  info.rel_path = rel_path;
  info.bytes = len;
  info.sha256 = digest;
  info.sha256_hex = crypto::Sha256::to_hex(digest);
  return Result<SegmentInfo>::Ok(std::move(info));
}

}  // namespace aegis::evidence
