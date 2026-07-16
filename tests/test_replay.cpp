#include "mdp/book.hpp"
#include "mdp/feed_handler.hpp"
#include "mdp/parser.hpp"
#include "mdp/protocol.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::vector<std::byte> encode_add(uint64_t seq, uint64_t oid, uint32_t sym, mdp::Side side,
                                  uint32_t px, uint32_t qty) {
  mdp::WireAddOrder msg{};
  mdp::encode_add(msg, seq, seq * 100, oid, sym, side, px, qty);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  return buf;
}

std::vector<std::byte> encode_del(uint64_t seq, uint64_t oid, uint32_t sym) {
  mdp::WireDeleteOrder msg{};
  mdp::encode_delete(msg, seq, seq * 100, oid, sym);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  return buf;
}

}  // namespace

TEST(Replay, CapturedSessionMatchesBook) {
  const auto dir = fs::temp_directory_path() / "mdp_replay_test";
  fs::create_directories(dir);
  const auto capture = dir / "session.capture";

  std::vector<std::vector<std::byte>> frames;
  frames.push_back(encode_add(1, 1, 0, mdp::Side::Buy, 100, 10));
  frames.push_back(encode_add(2, 2, 0, mdp::Side::Sell, 105, 8));
  frames.push_back(encode_add(3, 3, 0, mdp::Side::Buy, 101, 4));
  frames.push_back(encode_del(4, 3, 0));

  {
    std::ofstream out(capture, std::ios::binary);
    for (const auto& f : frames) {
      const uint32_t n = static_cast<uint32_t>(f.size());
      out.write(reinterpret_cast<const char*>(&n), sizeof(n));
      out.write(reinterpret_cast<const char*>(f.data()), static_cast<std::streamsize>(f.size()));
    }
  }

  mdp::BookBuilder expected;
  for (const auto& f : frames) {
    auto parsed = mdp::parse_message(f);
    ASSERT_EQ(parsed.status, mdp::ParseStatus::Ok);
    if (auto* add = std::get_if<mdp::AddOrder>(&parsed.payload)) {
      expected.on_add(*add);
    } else if (auto* del = std::get_if<mdp::DeleteOrder>(&parsed.payload)) {
      expected.on_delete(*del);
    }
  }
  const auto want = expected.top(0);

  mdp::HandlerConfig cfg;
  cfg.bbo_path = (dir / "bbo.log").string();
  cfg.tape_path = (dir / "tape.log").string();
  cfg.bars_path = (dir / "bars.bin").string();
  mdp::FeedHandler handler(cfg);

  std::ifstream in(capture, std::ios::binary);
  while (in) {
    uint32_t n = 0;
    in.read(reinterpret_cast<char*>(&n), sizeof(n));
    if (!in) {
      break;
    }
    std::vector<std::byte> frame(n);
    in.read(reinterpret_cast<char*>(frame.data()), n);
    handler.process_datagram(frame, mdp::now_ns());
  }

  const auto got = handler.book().top(0);
  EXPECT_EQ(got.bid_price, want.bid_price);
  EXPECT_EQ(got.bid_qty, want.bid_qty);
  EXPECT_EQ(got.ask_price, want.ask_price);
  EXPECT_EQ(got.ask_qty, want.ask_qty);
  EXPECT_EQ(got.bid_price, 100u);
  EXPECT_EQ(got.ask_price, 105u);

  fs::remove_all(dir);
}
