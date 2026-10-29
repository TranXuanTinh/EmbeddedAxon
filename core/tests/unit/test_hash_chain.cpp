#include <gtest/gtest.h>

#include <vector>

#include "aegis/crypto/hash_chain.h"
#include "aegis/crypto/sha256.h"

namespace aegis::crypto {
namespace {

TEST(HashChainTest, StartsAt32ZeroBytesGenesis) {
  HashChain chain;
  EXPECT_EQ(chain.length(), 0U);
  EXPECT_EQ(chain.head_hex(),
            "0000000000000000000000000000000000000000000000000000000000000000");
}

TEST(HashChainTest, DeterministicChainingAndOrderSensitivity) {
  const auto seg1 = Sha256::hash("segment-1");
  const auto seg2 = Sha256::hash("segment-2");

  HashChain chain_a;
  const auto c1 = chain_a.advance(seg1);
  const auto c2 = chain_a.advance(seg2);
  EXPECT_EQ(chain_a.length(), 2U);

  // Manual verification: chain_1 = SHA256(32 zero bytes || raw(seg1))
  std::vector<uint8_t> raw1(32, 0);
  raw1.insert(raw1.end(), seg1.begin(), seg1.end());
  EXPECT_EQ(c1, Sha256::hash(raw1));

  // Manual verification: chain_2 = SHA256(raw(c1) || raw(seg2))
  std::vector<uint8_t> raw2(c1.begin(), c1.end());
  raw2.insert(raw2.end(), seg2.begin(), seg2.end());
  EXPECT_EQ(c2, Sha256::hash(raw2));

  // Swapping order must yield a completely different chain head
  HashChain chain_b;
  (void)chain_b.advance(seg2);
  (void)chain_b.advance(seg1);
  EXPECT_NE(chain_a.head(), chain_b.head());
}

}  // namespace
}  // namespace aegis::crypto
