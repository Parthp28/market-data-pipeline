#include "mdp/feed_handler.hpp"
#include "mdp/parser.hpp"
#include "mdp/protocol.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::vector<std::byte> hb(uint64_t seq) {
  mdp::WireHeartbeat msg{};
  mdp::encode_heartbeat(msg, seq, seq, seq);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  return buf;
}

std::vector<std::byte> add(uint64_t seq, uint64_t oid, uint32_t px) {
  mdp::WireAddOrder msg{};
  mdp::encode_add(msg, seq, seq, oid, 0, mdp::Side::Buy, px, 5);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  return buf;
}

std::vector<std::byte> mod(uint64_t seq, uint64_t oid, uint32_t px) {
  mdp::WireModifyOrder msg{};
  mdp::encode_modify(msg, seq, seq, oid, 0, mdp::Side::Buy, px, 3);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  return buf;
}

std::vector<std::byte> trade(uint64_t seq) {
  mdp::WireTrade msg{};
  mdp::encode_trade(msg, seq, seq, seq, 0, mdp::Side::Sell, 100, 2);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  return buf;
}

}  // namespace

TEST(Parser, ParsesModifyDeleteTradeGap) {
  {
    mdp::WireModifyOrder msg{};
    mdp::encode_modify(msg, 1, 2, 3, 4, mdp::Side::Sell, 10, 11);
    std::vector<std::byte> buf(sizeof(msg));
    std::memcpy(buf.data(), &msg, sizeof(msg));
    auto p = mdp::parse_message(buf);
    ASSERT_EQ(p.status, mdp::ParseStatus::Ok);
    EXPECT_EQ(std::get<mdp::ModifyOrder>(p.payload).qty, 11u);
  }
  {
    mdp::WireDeleteOrder msg{};
    mdp::encode_delete(msg, 1, 2, 3, 4);
    std::vector<std::byte> buf(sizeof(msg));
    std::memcpy(buf.data(), &msg, sizeof(msg));
    auto p = mdp::parse_message(buf);
    ASSERT_EQ(p.status, mdp::ParseStatus::Ok);
    EXPECT_EQ(std::get<mdp::DeleteOrder>(p.payload).order_id, 3u);
  }
  {
    mdp::WireTrade msg{};
    mdp::encode_trade(msg, 1, 2, 9, 0, mdp::Side::Buy, 7, 8);
    std::vector<std::byte> buf(sizeof(msg));
    std::memcpy(buf.data(), &msg, sizeof(msg));
    auto p = mdp::parse_message(buf);
    ASSERT_EQ(p.status, mdp::ParseStatus::Ok);
    EXPECT_EQ(std::get<mdp::Trade>(p.payload).trade_id, 9u);
  }
  {
    mdp::WireGapRequest msg{};
    mdp::encode_gap_request(msg, 1, 2, 3, 4);
    std::vector<std::byte> buf(sizeof(msg));
    std::memcpy(buf.data(), &msg, sizeof(msg));
    auto p = mdp::parse_message(buf);
    ASSERT_EQ(p.status, mdp::ParseStatus::Ok);
    EXPECT_EQ(std::get<mdp::GapRequest>(p.payload).end_seq, 4u);
  }
  {
    mdp::WireGapResponse msg{};
    mdp::encode_gap_response(msg, 1, 2, 3, 4, 12);
    std::vector<std::byte> buf(sizeof(msg));
    std::memcpy(buf.data(), &msg, sizeof(msg));
    auto p = mdp::parse_message(buf);
    ASSERT_EQ(p.status, mdp::ParseStatus::Ok);
    EXPECT_EQ(std::get<mdp::GapResponse>(p.payload).payload_bytes, 12u);
  }
}

TEST(Handler, ProcessDatagramBuildsBookAndConsumes) {
  const auto dir = fs::temp_directory_path() / "mdp_handler_unit";
  fs::create_directories(dir);
  mdp::HandlerConfig cfg;
  cfg.bbo_path = (dir / "bbo.log").string();
  cfg.tape_path = (dir / "tape.log").string();
  cfg.bars_path = (dir / "bars.bin").string();
  cfg.print_stats = false;
  mdp::FeedHandler handler(cfg);

  handler.process_datagram(add(1, 1, 100), mdp::now_ns());
  handler.process_datagram(mod(2, 1, 101), mdp::now_ns());
  handler.process_datagram(trade(3), mdp::now_ns());
  handler.process_datagram(hb(4), mdp::now_ns());

  auto top = handler.book().top(0);
  EXPECT_EQ(top.bid_price, 101u);
  EXPECT_GE(handler.stats().messages, 3u);
  fs::remove_all(dir);
}

TEST(Handler, RecoveryFillsGap) {
  const auto dir = fs::temp_directory_path() / "mdp_recovery_unit";
  fs::create_directories(dir);

  mdp::SimulatorConfig scfg;
  scfg.messages = 20;
  scfg.seed = 123;
  scfg.udp_port = 19111;
  scfg.recovery_port = 19112;
  scfg.quiet = true;
  scfg.rate_per_sec = 0;
  mdp::FeedSimulator sim(scfg);
  auto session = sim.generate_session();

  std::atomic<bool> stop{false};
  std::thread rec([&] { mdp::run_recovery_server(19112, session, stop); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  mdp::HandlerConfig cfg;
  cfg.bbo_path = (dir / "bbo.log").string();
  cfg.tape_path = (dir / "tape.log").string();
  cfg.bars_path = (dir / "bars.bin").string();
  cfg.recovery_port = 19112;
  mdp::FeedHandler handler(cfg);

  // Deliver 1 then 3, forcing gap at 2 and TCP recovery.
  handler.process_datagram(session[0], mdp::now_ns());
  handler.process_datagram(session[2], mdp::now_ns());
  EXPECT_GE(handler.seq_stats().recovery_events, 1u);
  EXPECT_GE(handler.seq_stats().next_expected, 3u);

  stop.store(true);
  rec.join();
  fs::remove_all(dir);
}

TEST(Handler, UdpRunLoopBrief) {
  const auto dir = fs::temp_directory_path() / "mdp_udp_run";
  fs::create_directories(dir);

  mdp::HandlerConfig cfg;
  cfg.udp_port = 19331;
  cfg.recovery_port = 19332;
  cfg.run_seconds = 1;
  cfg.bbo_path = (dir / "bbo.log").string();
  cfg.tape_path = (dir / "tape.log").string();
  cfg.bars_path = (dir / "bars.bin").string();
  cfg.print_stats = true;

  mdp::FeedHandler handler(cfg);
  std::thread th([&] { EXPECT_EQ(handler.run(), 0); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  ASSERT_GE(fd, 0);
  sockaddr_in dest{};
  dest.sin_family = AF_INET;
  dest.sin_port = htons(19331);
  dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  auto frame = add(1, 42, 120);
  ::sendto(fd, frame.data(), frame.size(), 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
  frame = trade(2);
  ::sendto(fd, frame.data(), frame.size(), 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
  ::close(fd);

  th.join();
  EXPECT_GE(handler.stats().messages, 1u);
  fs::remove_all(dir);
}

TEST(Simulator, LossyRunCompletes) {
  mdp::SimulatorConfig cfg;
  cfg.messages = 30;
  cfg.udp_port = 19221;
  cfg.recovery_port = 19222;
  cfg.loss_rate = 0.1;
  cfg.reorder_rate = 0.1;
  cfg.rate_per_sec = 100000;
  cfg.quiet = true;
  cfg.seed = 9;
  mdp::FeedSimulator sim(cfg);
  EXPECT_EQ(sim.run(), 0);
}
