#ifndef AEGIS_EVIDENCE_EVIDENCE_STORE_H_
#define AEGIS_EVIDENCE_EVIDENCE_STORE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aegis/common/result.h"
#include "aegis/crypto/hash_chain.h"
#include "aegis/evidence/frame.h"
#include "aegis/evidence/segment_writer.h"
#include "aegis/hal/i_key_store_hal.h"
#include "aegis/hal/i_storage_hal.h"

namespace aegis::evidence {

struct ManifestSegmentEntry {
  uint32_t index{0};
  std::string file;
  std::size_t bytes{0};
  std::string sha256;
  std::string chain;
  bool uploaded{false};
};

struct IncidentManifest {
  uint32_t version{1};
  std::string device_id;
  std::string incident_id;
  std::string trigger_source;
  uint64_t pre_event_ms{30000};
  std::vector<ManifestSegmentEntry> segments;
  std::string chain_head;
  std::string hmac;
  bool sealed{false};
};

class EvidenceStore {
 public:
  EvidenceStore(hal::IStorageHal& storage, hal::IKeyStoreHal& key_store,
                std::string device_id, uint64_t pre_event_ms = 30000);

  // Starts a new incident: INC-<device_id>-<start_ts_ms> and writes initial
  // unsealed manifest.json.
  Result<std::string> begin_incident(uint64_t start_ts_ms,
                                     std::string_view trigger_source);

  // Writes the next segment of frames, updates the hash chain + HMAC, and
  // persists manifest.json atomically.
  Result<ManifestSegmentEntry> append_segment(std::string_view incident_id,
                                              const std::vector<Frame>& frames);

  // Seals the incident and persists manifest.json with "sealed": true.
  Status seal_incident(std::string_view incident_id);

  // Marks a segment as uploaded in memory (tracked via UPLOAD_ACK).
  Status mark_segment_uploaded(std::string_view incident_id,
                               uint32_t segment_index);

  // Verifies a single incident's manifest, segment files, hash chain, and HMAC.
  Status verify_incident(std::string_view incident_id) const;

  // Verifies all incidents under <store>/incidents/.
  Status verify_all() const;

  // Power-loss recovery: cleans up orphaned .tmp files and seals any unsealed
  // incident manifests left from an unclean shutdown.
  Status recover();

  Result<IncidentManifest> get_manifest(std::string_view incident_id) const;

  static std::string serialize_manifest(const IncidentManifest& manifest);
  static Result<IncidentManifest> parse_manifest(std::string_view json);

 private:
  struct ActiveIncident {
    IncidentManifest manifest;
    crypto::HashChain chain;
  };

  Status persist_manifest(const IncidentManifest& manifest);

  hal::IStorageHal& storage_;
  hal::IKeyStoreHal& key_store_;
  std::string device_id_;
  uint64_t pre_event_ms_;
  SegmentWriter writer_;
  std::unordered_map<std::string, ActiveIncident> incidents_;
};

}  // namespace aegis::evidence

#endif  // AEGIS_EVIDENCE_EVIDENCE_STORE_H_
