#include "mdp/consumers.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

namespace {

std::string tmp_path(const char* name) {
  return std::string("/tmp/mdp_test_") + name;
}

}  // namespace

TEST(Consumers, BboAndTapeWriteLines) {
  const auto bbo_path = tmp_path("bbo.log");
  const auto tape_path = tmp_path("tape.log");
  {
    mdp::BboPublisher bbo(bbo_path);
    mdp::TradeTapeWriter tape(tape_path);

    mdp::NormalizedTick tick{};
    tick.ts_ns = 11;
    tick.symbol_id = 1;
    tick.bid_price = 100;
    tick.bid_qty = 2;
    tick.ask_price = 101;
    tick.ask_qty = 3;
    tick.seq = 5;
    bbo.on_tick(tick);

    mdp::TradeEvent tr{};
    tr.ts_ns = 12;
    tr.symbol_id = 1;
    tr.price = 101;
    tr.qty = 1;
    tr.side = mdp::Side::Buy;
    tr.trade_id = 9;
    tr.seq = 6;
    tape.on_trade(tr);

    EXPECT_EQ(bbo.count(), 1u);
    EXPECT_EQ(tape.count(), 1u);
  }

  std::ifstream in(bbo_path);
  std::string line;
  ASSERT_TRUE(static_cast<bool>(std::getline(in, line)));
  EXPECT_NE(line.find("100"), std::string::npos);
  std::remove(bbo_path.c_str());
  std::remove(tape_path.c_str());
}

TEST(Consumers, VolumeBarsFlush) {
  const auto path = tmp_path("bars.bin");
  mdp::BarAggregator bars(path, 1'000'000'000ull, 10);
  mdp::TradeEvent tr{};
  tr.symbol_id = 0;
  tr.ts_ns = 100;
  tr.price = 50;
  tr.qty = 6;
  bars.on_trade(tr);
  tr.ts_ns = 200;
  tr.qty = 6;
  bars.on_trade(tr);
  bars.flush_all();
  EXPECT_GE(bars.volume_bars(), 1u);

  std::ifstream in(path, std::ios::binary);
  mdp::BinaryBar bar{};
  ASSERT_TRUE(static_cast<bool>(in.read(reinterpret_cast<char*>(&bar), sizeof(bar))));
  EXPECT_EQ(bar.symbol_id, 0u);
  EXPECT_EQ(bar.bar_type, 1);
  std::remove(path.c_str());
}
