#ifndef AEGIS_CRYPTO_SHA256_H_
#define AEGIS_CRYPTO_SHA256_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aegis/common/result.h"

namespace aegis::crypto {

class Sha256 {
 public:
  static constexpr std::size_t kDigestSize = 32;
  static constexpr std::size_t kBlockSize = 64;
  using Digest = std::array<uint8_t, kDigestSize>;

  Sha256() noexcept;

  void reset() noexcept;
  void update(const uint8_t* data, std::size_t len) noexcept;
  void update(std::string_view data) noexcept;
  void update(const std::vector<uint8_t>& data) noexcept;
  Digest final() noexcept;

  static Digest hash(const uint8_t* data, std::size_t len) noexcept;
  static Digest hash(std::string_view data) noexcept;
  static Digest hash(const std::vector<uint8_t>& data) noexcept;

  static std::string hash_hex(const uint8_t* data, std::size_t len);
  static std::string hash_hex(std::string_view data);
  static std::string hash_hex(const std::vector<uint8_t>& data);

  static std::string to_hex(const Digest& digest);
  static Result<Digest> from_hex(std::string_view hex);

 private:
  void transform(const uint8_t block[kBlockSize]) noexcept;

  std::array<uint32_t, 8> state_{};
  std::array<uint8_t, kBlockSize> buffer_{};
  uint64_t bit_len_{0};
  std::size_t buffer_len_{0};
  bool finalized_{false};
};

using Sha256Digest = Sha256::Digest;

}  // namespace aegis::crypto

#endif  // AEGIS_CRYPTO_SHA256_H_
