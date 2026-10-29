#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "aegis/crypto/sha256.h"

namespace aegis::crypto {
namespace {

TEST(Sha256Test, EmptyStringMatchesFips180Vector) {
  EXPECT_EQ(Sha256::hash_hex(""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Test, AbcMatchesFips180Vector) {
  EXPECT_EQ(Sha256::hash_hex("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, MultiBlock448BitMessageMatchesNistVector) {
  const std::string msg =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  EXPECT_EQ(Sha256::hash_hex(msg),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Sha256Test, StreamingChunkedUpdateMatchesOneShot) {
  const std::string full =
      "The quick brown fox jumps over the lazy dog. Deterministic evidence!";
  Sha256 stream;
  for (char c : full) {
    stream.update(reinterpret_cast<const uint8_t*>(&c), 1);
  }
  const auto digest = stream.final();
  EXPECT_EQ(Sha256::to_hex(digest), Sha256::hash_hex(full));
}

TEST(Sha256Test, HexRoundTripAndValidation) {
  const std::string expected =
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
  auto parsed = Sha256::from_hex(expected);
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(Sha256::to_hex(parsed.value()), expected);

  EXPECT_FALSE(Sha256::from_hex("short").ok());
  const std::string bad_chars(64, 'z');
  EXPECT_FALSE(Sha256::from_hex(bad_chars).ok());
}

}  // namespace
}  // namespace aegis::crypto
