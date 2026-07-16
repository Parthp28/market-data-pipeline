#include "mdp/book.hpp"
#include "mdp/bytes.hpp"
#include "mdp/protocol.hpp"
#include "mdp/sequence_tracker.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

TEST(CoverageExtras, BytesHelpersAndBadBookInputs) {
  uint8_t buf[8];
  mdp::store_be16(buf, 0xA1B2);
  mdp::store_be32(buf, 0x01020304u);
  mdp::store_be64(buf, 0x0102030405060708ull);
  EXPECT_EQ(mdp::load_be16(buf), 0x0102u);
  EXPECT_EQ(mdp::load_be32(buf), 0x01020304u);
  EXPECT_EQ(mdp::load_be64(buf), 0x0102030405060708ull);

  mdp::BookBuilder book;
  mdp::AddOrder bad{};
  bad.symbol_id = 999;
  bad.price = 1;
  bad.qty = 1;
  EXPECT_FALSE(book.on_add(bad).has_value());
  bad.symbol_id = 0;
  bad.price = 0;
  EXPECT_FALSE(book.on_add(bad).has_value());
  bad.price = 1;
  bad.qty = 0;
  EXPECT_FALSE(book.on_add(bad).has_value());

  mdp::ModifyOrder m{};
  m.order_id = 12345;
  EXPECT_FALSE(book.on_modify(m).has_value());
  EXPECT_FALSE(book.on_delete(mdp::DeleteOrder{}).has_value());
  mdp::Trade t{};
  t.symbol_id = 0;
  t.price = 0;
  t.qty = 1;
  EXPECT_FALSE(book.on_trade(t).has_value());
  EXPECT_EQ(book.bid_qty_at(0, 0), 0u);
  EXPECT_EQ(book.ask_qty_at(99, 1), 0u);
  EXPECT_FALSE(book.top(99).valid);
}

TEST(CoverageExtras, SequenceWindowConflictAndEncodeBytes) {
  mdp::SequenceTracker tr;
  auto make = [](uint64_t seq) {
    mdp::WireHeartbeat msg{};
    mdp::encode_heartbeat(msg, seq, seq, seq);
    std::vector<std::byte> buf(sizeof(msg));
    std::memcpy(buf.data(), &msg, sizeof(msg));
    return buf;
  };
  tr.push(make(1));
  tr.clear_ready();
  tr.push(make(3));
  // Same reorder slot as seq 3 with different seq later (3 and 3+64).
  tr.push(make(3 + 64));
  EXPECT_GE(tr.stats().drops, 1u);
  tr.note_recovery(100);
  EXPECT_EQ(tr.stats().recovery_events, 1u);

  std::byte out[64];
  std::size_t wrote = 0;
  EXPECT_TRUE(mdp::encode_message_bytes(std::span<std::byte>(out, 64), mdp::MsgType::Heartbeat, 1,
                                        2, nullptr, 0, &wrote));
  EXPECT_EQ(wrote, sizeof(mdp::WireHeartbeat));
  EXPECT_FALSE(mdp::encode_message_bytes(std::span<std::byte>(out, 4), mdp::MsgType::Heartbeat, 1, 2,
                                         nullptr, 0, &wrote));
}

TEST(CoverageExtras, NonBboChangeAddSameLevel) {
  mdp::BookBuilder book;
  mdp::AddOrder a{};
  a.hdr.seq = 1;
  a.order_id = 1;
  a.symbol_id = 0;
  a.side = mdp::Side::Buy;
  a.price = 50;
  a.qty = 1;
  EXPECT_TRUE(book.on_add(a).has_value());
  a.order_id = 2;
  a.hdr.seq = 2;
  a.price = 40;  // below best, should not publish BBO change? actually bid best stays 50, qty same so nullopt
  auto tick = book.on_add(a);
  EXPECT_FALSE(tick.has_value());
  EXPECT_EQ(book.bid_qty_at(0, 40), 1u);
}
