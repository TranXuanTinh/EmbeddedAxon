#ifndef AEGIS_CRYPTO_HMAC_H_
#define AEGIS_CRYPTO_HMAC_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aegis/crypto/sha256.h"

namespace aegis::crypto {

class HmacSha256 {
 public:
  using Digest = Sha256::Digest;

  static Digest compute(const uint8_t* key, std::size_t key_len,
                        const uint8_t* msg, std::size_t msg_len) noexcept;

  static Digest compute(std::string_view key, std::string_view msg) noexcept;

  static Digest compute(const std::vector<uint8_t>& key,
                        std::string_view msg) noexcept;

  static std::string compute_hex(const uint8_t* key, std::size_t key_len,
                                 const uint8_t* msg, std::size_t msg_len);

  static std::string compute_hex(std::string_view key, std::string_view msg);

  static std::string compute_hex(const std::vector<uint8_t>& key,
                                 std::string_view msg);

  static bool verify_constant_time(const Digest& expected,
                                   const Digest& actual) noexcept;
};

}  // namespace aegis::crypto

#endif  // AEGIS_CRYPTO_HMAC_H_
