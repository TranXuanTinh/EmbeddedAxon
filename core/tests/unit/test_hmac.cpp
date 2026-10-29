#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "aegis/crypto/hmac.h"

namespace aegis::crypto {
namespace {

// RFC 4231 Test Case 1: key = 20 bytes of 0x0b, data = "Hi There"
TEST(HmacSha256Test, Rfc4231Vector1) {
  const std::string key(20, '\x0b');
  const std::string data = "Hi There";
  EXPECT_EQ(HmacSha256::compute_hex(key, data),
            "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
}

// RFC 4231 Test Case 2: key = "Jefe", data = "what do ya want for nothing?"
TEST(HmacSha256Test, Rfc4231Vector2) {
  EXPECT_EQ(HmacSha256::compute_hex("Jefe", "what do ya want for nothing?"),
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

// RFC 4231 Test Case 6: key > 64 bytes (131 bytes of 0xaa)
TEST(HmacSha256Test, Rfc4231LongKeyVector6) {
  const std::string key(131, '\xaa');
  const std::string data =
      "Test Using Larger Than Block-Size Key - Hash Key First";
  EXPECT_EQ(HmacSha256::compute_hex(key, data),
            "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
}

TEST(HmacSha256Test, ConstantTimeVerifyDetectsBitFlip) {
  const auto mac1 = HmacSha256::compute("secret-key", "manifest-head");
  auto mac2 = mac1;
  EXPECT_TRUE(HmacSha256::verify_constant_time(mac1, mac2));
  mac2[15] ^= 0x01U;
  EXPECT_FALSE(HmacSha256::verify_constant_time(mac1, mac2));
}

}  // namespace
}  // namespace aegis::crypto
