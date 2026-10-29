#ifndef AEGIS_EVIDENCE_AUDIT_LOG_H_
#define AEGIS_EVIDENCE_AUDIT_LOG_H_

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

#include "aegis/common/clock.h"
#include "aegis/common/result.h"
#include "aegis/hal/i_storage_hal.h"

namespace aegis::evidence {

// Append-only hash-chained audit log per spec §4:
// Line format: <seq>|<ts_ms>|<event>|<detail>|<prev_hash_hex>|<hash_hex>
// hash = SHA256(prev_hash_hex || "|" || seq || "|" || ts_ms || "|" || event || "|" || detail)
// Genesis prev_hash_hex = 64 '0' characters.
class AuditLog {
 public:
  static constexpr std::string_view kDefaultRelPath = "audit.log";
  static constexpr std::string_view kGenesisPrevHash =
      "0000000000000000000000000000000000000000000000000000000000000000";

  AuditLog(hal::IStorageHal& storage, const common::IClock& clock,
           std::string rel_path = std::string(kDefaultRelPath));

  // Loads existing audit.log (if present) to resume sequence and hash chain.
  Status init();

  // Appends a new hash-chained audit record and flushes to storage.
  Status append(std::string_view event, std::string_view detail);

  // Verifies the entire audit.log file from genesis.
  Status verify() const;

  uint64_t last_seq() const;
  std::string last_hash_hex() const;

  static std::string compute_record_hash(std::string_view prev_hash_hex,
                                         uint64_t seq, uint64_t ts_ms,
                                         std::string_view event,
                                         std::string_view detail);

 private:
  hal::IStorageHal& storage_;
  const common::IClock& clock_;
  std::string rel_path_;
  mutable std::mutex mutex_;
  uint64_t seq_{0};
  std::string prev_hash_hex_{kGenesisPrevHash};
};

}  // namespace aegis::evidence

#endif  // AEGIS_EVIDENCE_AUDIT_LOG_H_
