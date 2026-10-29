#ifndef AEGIS_EVIDENCE_FRAME_H_
#define AEGIS_EVIDENCE_FRAME_H_

#include <cstdint>
#include <vector>

namespace aegis::evidence {

struct Frame {
  uint64_t seq{0};
  uint64_t ts_ms{0};
  bool keyframe{false};
  std::vector<uint8_t> payload;
};

}  // namespace aegis::evidence

#endif  // AEGIS_EVIDENCE_FRAME_H_
