#include "mdp/book.hpp"
#include "mdp/protocol.hpp"

#include <gtest/gtest.h>

TEST(Book, AddImprovesBid) {
  mdp::BookBuilder book;
  mdp::AddOrder a{};
  a.hdr.seq = 1;
  a.hdr.ts_ns = 10;
  a.order_id = 1;
  a.symbol_id = 0;
  a.side = mdp::Side::Buy;
  a.price = 100;
  a.qty = 5;
  auto tick = book.on_add(a);
  ASSERT_TRUE(tick.has_value());
  EXPECT_EQ(tick->bid_price, 100u);
  EXPECT_EQ(tick->bid_qty, 5u);

  a.order_id = 2;
  a.price = 101;
  a.qty = 3;
  a.hdr.seq = 2;
  tick = book.on_add(a);
  ASSERT_TRUE(tick.has_value());
  EXPECT_EQ(tick->bid_price, 101u);
  EXPECT_EQ(tick->bid_qty, 3u);
}

TEST(Book, DeleteRestoresLowerBid) {
  mdp::BookBuilder book;
  mdp::AddOrder a{};
  a.hdr = {mdp::MsgType::AddOrder, 0, 1, 1};
  a.order_id = 1;
  a.symbol_id = 1;
  a.side = mdp::Side::Buy;
  a.price = 100;
  a.qty = 5;
  book.on_add(a);
  a.order_id = 2;
  a.price = 105;
  a.qty = 2;
  a.hdr.seq = 2;
  book.on_add(a);

  mdp::DeleteOrder d{};
  d.hdr = {mdp::MsgType::DeleteOrder, 0, 3, 3};
  d.order_id = 2;
  d.symbol_id = 1;
  auto tick = book.on_delete(d);
  ASSERT_TRUE(tick.has_value());
  EXPECT_EQ(tick->bid_price, 100u);
  EXPECT_EQ(tick->bid_qty, 5u);
}

TEST(Book, ModifyMovesLevel) {
  mdp::BookBuilder book;
  mdp::AddOrder a{};
  a.hdr = {mdp::MsgType::AddOrder, 0, 1, 1};
  a.order_id = 9;
  a.symbol_id = 0;
  a.side = mdp::Side::Sell;
  a.price = 200;
  a.qty = 10;
  book.on_add(a);

  mdp::ModifyOrder m{};
  m.hdr = {mdp::MsgType::ModifyOrder, 0, 2, 2};
  m.order_id = 9;
  m.symbol_id = 0;
  m.side = mdp::Side::Sell;
  m.price = 190;
  m.qty = 4;
  auto tick = book.on_modify(m);
  ASSERT_TRUE(tick.has_value());
  EXPECT_EQ(tick->ask_price, 190u);
  EXPECT_EQ(tick->ask_qty, 4u);
  EXPECT_EQ(book.ask_qty_at(0, 200), 0u);
}

TEST(Book, TradeUpdatesLast) {
  mdp::BookBuilder book;
  mdp::Trade t{};
  t.hdr = {mdp::MsgType::Trade, 0, 1, 1};
  t.trade_id = 1;
  t.symbol_id = 0;
  t.side = mdp::Side::Buy;
  t.price = 150;
  t.qty = 7;
  auto tick = book.on_trade(t);
  ASSERT_TRUE(tick.has_value());
  EXPECT_EQ(tick->last_price, 150u);
  EXPECT_EQ(tick->last_qty, 7u);
}
