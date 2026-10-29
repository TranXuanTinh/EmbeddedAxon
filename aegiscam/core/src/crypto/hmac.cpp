#include "aegis/crypto/hmac.h"

#include <array>
#include <cstring>

namespace aegis::crypto {

HmacSha256::Digest HmacSha256::compute(const uint8_t* key, std::size_t key_len,
                                       const uint8_t* msg,
                                       std::size_t msg_len) noexcept {
  std::array<uint8_t, Sha256::kBlockSize> key_block{};
  key_block.fill(0);

  if (key_len > Sha256::kBlockSize) {
    const auto hashed_key = Sha256::hash(key, key_len);
    std::memcpy(key_block.data(), hashed_key.data(), hashed_key.size());
  } else if (key != nullptr && key_len > 0) {
    std::memcpy(key_block.data(), key, key_len);
  }

  std::array<uint8_t, Sha256::kBlockSize> o_key_pad{};
  std::array<uint8_t, Sha256::kBlockSize> i_key_pad{};
  for (std::size_t i = 0; i < Sha256::kBlockSize; ++i) {
    o_key_pad[i] = static_cast<uint8_t>(key_block[i] ^ 0x5cU);
    i_key_pad[i] = static_cast<uint8_t>(key_block[i] ^ 0x36U);
  }

  Sha256 inner;
  inner.update(i_key_pad.data(), i_key_pad.size());
  inner.update(msg, msg_len);
  const auto inner_digest = inner.final();

  Sha256 outer;
  outer.update(o_key_pad.data(), o_key_pad.size());
  outer.update(inner_digest.data(), inner_digest.size());
  return outer.final();
}

HmacSha256::Digest HmacSha256::compute(std::string_view key,
                                       std::string_view msg) noexcept {
  return compute(reinterpret_cast<const uint8_t*>(key.data()), key.size(),
                 reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
}

HmacSha256::Digest HmacSha256::compute(const std::vector<uint8_t>& key,
                                       std::string_view msg) noexcept {
  return compute(key.data(), key.size(),
                 reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
}

std::string HmacSha256::compute_hex(const uint8_t* key, std::size_t key_len,
                                    const uint8_t* msg, std::size_t msg_len) {
  return Sha256::to_hex(compute(key, key_len, msg, msg_len));
}

std::string HmacSha256::compute_hex(std::string_view key,
                                    std::string_view msg) {
  return Sha256::to_hex(compute(key, msg));
}

std::string HmacSha256::compute_hex(const std::vector<uint8_t>& key,
                                    std::string_view msg) {
  return Sha256::to_hex(compute(key, msg));
}

bool HmacSha256::verify_constant_time(const Digest& expected,
                                      const Digest& actual) noexcept {
  uint8_t diff = 0;
  for (std::size_t i = 0; i < Sha256::kDigestSize; ++i) {
    diff |= static_cast<uint8_t>(expected[i] ^ actual[i]);
  }
  return diff == 0;
}

}  // namespace aegis::crypto
