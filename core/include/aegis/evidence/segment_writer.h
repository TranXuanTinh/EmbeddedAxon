#ifndef AEGIS_EVIDENCE_SEGMENT_WRITER_H_
#define AEGIS_EVIDENCE_SEGMENT_WRITER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aegis/common/result.h"
#include "aegis/crypto/sha256.h"
#include "aegis/evidence/frame.h"
#include "aegis/hal/i_storage_hal.h"

namespace aegis::evidence {

struct SegmentInfo {
  uint32_t index{0};
  std::string file;
  std::string rel_path;
  std::size_t bytes{0};
  crypto::Sha256::Digest sha256{};
  std::string sha256_hex;
};

class SegmentWriter {
 public:
  explicit SegmentWriter(hal::IStorageHal& storage) noexcept;

  // Serializes frames into deterministic binary format and writes
  // incidents/<incident_id>/seg_%06u.bin atomically.
  Result<SegmentInfo> write_segment(std::string_view incident_id,
                                    uint32_t segment_index,
                                    const std::vector<Frame>& frames);

  // Writes raw bytes directly as a segment file.
  Result<SegmentInfo> write_raw_segment(std::string_view incident_id,
                                        uint32_t segment_index,
                                        const uint8_t* data, std::size_t len);

  static std::string format_segment_filename(uint32_t segment_index);
  static std::vector<uint8_t> serialize_frames(
      const std::vector<Frame>& frames);

 private:
  hal::IStorageHal& storage_;
};

}  // namespace aegis::evidence

#endif  // AEGIS_EVIDENCE_SEGMENT_WRITER_H_
