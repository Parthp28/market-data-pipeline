#include "mdp/parser.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

TEST(FuzzParser, RandomBytesDoNotCrash) {
  std::mt19937_64 rng(0xC0FFEEu);
  std::uniform_int_distribution<int> len_dist(0, 64);
  std::uniform_int_distribution<int> byte_dist(0, 255);

  for (int i = 0; i < 20000; ++i) {
    const int n = len_dist(rng);
    std::vector<std::byte> buf(static_cast<std::size_t>(n));
    for (auto& b : buf) {
      b = static_cast<std::byte>(byte_dist(rng));
    }
    const auto parsed = mdp::parse_message(buf);
    EXPECT_TRUE(parsed.status == mdp::ParseStatus::Ok ||
                parsed.status == mdp::ParseStatus::Truncated ||
                parsed.status == mdp::ParseStatus::BadLength ||
                parsed.status == mdp::ParseStatus::UnknownType ||
                parsed.status == mdp::ParseStatus::BadSide);
    if (parsed.status == mdp::ParseStatus::Ok) {
      EXPECT_GT(parsed.consumed, 0u);
      EXPECT_LE(parsed.consumed, buf.size());
    }
  }
}
