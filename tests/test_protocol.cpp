#include "mdp/bytes.hpp"
#include "mdp/protocol.hpp"

#include <gtest/gtest.h>

#include <cstring>

TEST(Protocol, StructSizesAreFixed) {
  EXPECT_EQ(sizeof(mdp::WireHeader), 20u);
  EXPECT_EQ(sizeof(mdp::WireAddOrder), 44u);
  EXPECT_EQ(sizeof(mdp::WireModifyOrder), 44u);
  EXPECT_EQ(sizeof(mdp::WireDeleteOrder), 36u);
  EXPECT_EQ(sizeof(mdp::WireTrade), 44u);
  EXPECT_EQ(sizeof(mdp::WireHeartbeat), 28u);
  EXPECT_EQ(sizeof(mdp::WireGapRequest), 36u);
  EXPECT_EQ(sizeof(mdp::WireGapResponse), 44u);
}

TEST(Protocol, EndianRoundTrip) {
  uint16_t a = 0x1234;
  uint32_t b = 0x89ABCDEFu;
  uint64_t c = 0x0123456789ABCDEFull;
  uint8_t buf[14];
  mdp::store_be16(buf, a);
  mdp::store_be32(buf + 2, b);
  mdp::store_be64(buf + 6, c);
  EXPECT_EQ(mdp::load_be16(buf), a);
  EXPECT_EQ(mdp::load_be32(buf + 2), b);
  EXPECT_EQ(mdp::load_be64(buf + 6), c);
}

TEST(Protocol, EncodeAddOrderHeaderFields) {
  mdp::WireAddOrder msg{};
  mdp::encode_add(msg, 42, 99, 7, 3, mdp::Side::Sell, 1500, 25);
  const mdp::Header h = mdp::decode_header(msg.hdr);
  EXPECT_EQ(h.type, mdp::MsgType::AddOrder);
  EXPECT_EQ(h.seq, 42u);
  EXPECT_EQ(h.ts_ns, 99u);
  EXPECT_EQ(h.length, sizeof(mdp::WireAddOrder));
  EXPECT_EQ(mdp::load_be64(&msg.order_id), 7u);
  EXPECT_EQ(mdp::load_be32(&msg.symbol_id), 3u);
  EXPECT_EQ(msg.side, static_cast<uint8_t>(mdp::Side::Sell));
  EXPECT_EQ(mdp::load_be32(&msg.price), 1500u);
  EXPECT_EQ(mdp::load_be32(&msg.qty), 25u);
}

TEST(Protocol, WireLengthTable) {
  EXPECT_EQ(mdp::wire_length_for(mdp::MsgType::DeleteOrder), sizeof(mdp::WireDeleteOrder));
  EXPECT_EQ(mdp::wire_length_for(mdp::MsgType::Heartbeat), sizeof(mdp::WireHeartbeat));
  EXPECT_EQ(mdp::wire_length_for(static_cast<mdp::MsgType>(255)), 0);
}
