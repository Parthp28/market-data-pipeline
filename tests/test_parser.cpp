#include "mdp/parser.hpp"
#include "mdp/protocol.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

TEST(Parser, ParsesAddOrderFromBuffer) {
  mdp::WireAddOrder msg{};
  mdp::encode_add(msg, 10, 1000, 55, 2, mdp::Side::Buy, 1200, 40);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  const auto parsed = mdp::parse_message(buf);
  ASSERT_EQ(parsed.status, mdp::ParseStatus::Ok);
  ASSERT_TRUE(std::holds_alternative<mdp::AddOrder>(parsed.payload));
  const auto& add = std::get<mdp::AddOrder>(parsed.payload);
  EXPECT_EQ(add.order_id, 55u);
  EXPECT_EQ(add.symbol_id, 2u);
  EXPECT_EQ(add.side, mdp::Side::Buy);
  EXPECT_EQ(add.price, 1200u);
  EXPECT_EQ(add.qty, 40u);
  EXPECT_EQ(parsed.consumed, sizeof(msg));
}

TEST(Parser, RejectsTruncated) {
  mdp::WireTrade msg{};
  mdp::encode_trade(msg, 1, 1, 1, 0, mdp::Side::Buy, 1, 1);
  std::vector<std::byte> buf(sizeof(msg) - 4);
  std::memcpy(buf.data(), &msg, buf.size());
  const auto parsed = mdp::parse_message(buf);
  EXPECT_EQ(parsed.status, mdp::ParseStatus::Truncated);
}

TEST(Parser, RejectsUnknownType) {
  mdp::WireHeader wh{};
  wh.type = 250;
  mdp::store_be16(&wh.length, sizeof(mdp::WireHeader));
  mdp::store_be64(&wh.seq, 1);
  mdp::store_be64(&wh.ts_ns, 1);
  std::vector<std::byte> buf(sizeof(wh));
  std::memcpy(buf.data(), &wh, sizeof(wh));
  const auto parsed = mdp::parse_message(buf);
  EXPECT_EQ(parsed.status, mdp::ParseStatus::UnknownType);
}

TEST(Parser, RejectsBadSide) {
  mdp::WireAddOrder msg{};
  mdp::encode_add(msg, 1, 1, 1, 0, mdp::Side::Buy, 1, 1);
  msg.side = 9;
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  const auto parsed = mdp::parse_message(buf);
  EXPECT_EQ(parsed.status, mdp::ParseStatus::BadSide);
}

TEST(Parser, ParsesHeartbeat) {
  mdp::WireHeartbeat msg{};
  mdp::encode_heartbeat(msg, 3, 4, 5);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  const auto parsed = mdp::parse_message(buf);
  ASSERT_EQ(parsed.status, mdp::ParseStatus::Ok);
  const auto& hb = std::get<mdp::Heartbeat>(parsed.payload);
  EXPECT_EQ(hb.exchange_ts_ns, 5u);
}
