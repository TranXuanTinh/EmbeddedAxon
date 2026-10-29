#include "aegis/crypto/hash_chain.h"

namespace aegis::crypto {

HashChain::HashChain() noexcept { reset(); }

void HashChain::reset() noexcept {
  head_.fill(0);
  count_ = 0;
}

HashChain::Digest HashChain::compute_next(const Digest& prev,
                                          const Digest& segment_hash) noexcept {
  Sha256 hasher;
  hasher.update(prev.data(), prev.size());
  hasher.update(segment_hash.data(), segment_hash.size());
  return hasher.final();
}

HashChain::Digest HashChain::advance(const Digest& segment_hash) noexcept {
  head_ = compute_next(head_, segment_hash);
  ++count_;
  return head_;
}

}  // namespace aegis::crypto
