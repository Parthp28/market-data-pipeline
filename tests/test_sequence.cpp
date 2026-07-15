#include "mdp/protocol.hpp"
#include "mdp/sequence_tracker.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

namespace {

std::vector<std::byte> make_hb(uint64_t seq) {
  mdp::WireHeartbeat msg{};
  mdp::encode_heartbeat(msg, seq, seq * 10, seq);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  return buf;
}

}  // namespace

TEST(Sequence, InOrderAdvances) {
  mdp::SequenceTracker tr;
  tr.push(make_hb(1));
  ASSERT_EQ(tr.ready().size(), 1u);
  EXPECT_EQ(tr.stats().next_expected, 2u);
  tr.clear_ready();
  tr.push(make_hb(2));
  EXPECT_EQ(tr.stats().next_expected, 3u);
}

TEST(Sequence, OutOfOrderBufferedThenFlushed) {
  mdp::SequenceTracker tr;
  tr.push(make_hb(1));
  tr.clear_ready();
  tr.push(make_hb(3));
  EXPECT_TRUE(tr.gap_pending());
  EXPECT_EQ(tr.gap_start(), 2u);
  EXPECT_EQ(tr.gap_end(), 2u);
  EXPECT_TRUE(tr.ready().empty());
  tr.push(make_hb(2));
  ASSERT_EQ(tr.ready().size(), 2u);
  EXPECT_EQ(tr.stats().next_expected, 4u);
  EXPECT_FALSE(tr.gap_pending());
}

TEST(Sequence, DuplicateOldSeqCountedAsDrop) {
  mdp::SequenceTracker tr;
  tr.push(make_hb(1));
  tr.clear_ready();
  tr.push(make_hb(1));
  EXPECT_EQ(tr.stats().drops, 1u);
}
