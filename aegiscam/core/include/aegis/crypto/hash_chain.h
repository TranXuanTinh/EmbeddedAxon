#ifndef AEGIS_CRYPTO_HASH_CHAIN_H_
#define AEGIS_CRYPTO_HASH_CHAIN_H_

#include <cstddef>
#include <string>

#include "aegis/crypto/sha256.h"

namespace aegis::crypto {

// Hash chain per spec §3 and §4:
// H_0 = 32 zero bytes (genesis)
// H_n = SHA256(raw(H_{n-1}) || raw(segment_hash_n))
class HashChain {
 public:
  using Digest = Sha256::Digest;

  HashChain() noexcept;

  void reset() noexcept;

  // Advances the chain with a 32-byte raw segment hash and returns new head H_n.
  Digest advance(const Digest& segment_hash) noexcept;

  const Digest& head() const noexcept { return head_; }
  std::string head_hex() const { return Sha256::to_hex(head_); }
  std::size_t length() const noexcept { return count_; }

  static constexpr Digest genesis() noexcept { return Digest{}; }
  static Digest compute_next(const Digest& prev,
                             const Digest& segment_hash) noexcept;

 private:
  Digest head_{};
  std::size_t count_{0};
};

}  // namespace aegis::crypto

#endif  // AEGIS_CRYPTO_HASH_CHAIN_H_
