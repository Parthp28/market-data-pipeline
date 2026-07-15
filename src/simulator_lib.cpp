#include "mdp/feed_handler.hpp"

#include "mdp/bytes.hpp"
#include "mdp/parser.hpp"
#include "mdp/protocol.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>

namespace mdp {
namespace {

uint64_t host_now_ns() {
  using clock = std::chrono::system_clock;
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch())
          .count());
}

}  // namespace

FeedSimulator::FeedSimulator(SimulatorConfig cfg) : cfg_(std::move(cfg)) {}

std::vector<std::vector<std::byte>> FeedSimulator::generate_session() const {
  std::mt19937_64 rng(cfg_.seed);
  std::uniform_int_distribution<uint32_t> sym_dist(0, static_cast<uint32_t>(cfg_.symbols - 1));
  std::uniform_int_distribution<uint32_t> px_dist(1000, 2000);
  std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
  std::uniform_int_distribution<int> type_dist(0, 9);

  std::vector<std::vector<std::byte>> session;
  session.reserve(static_cast<std::size_t>(cfg_.messages));

  uint64_t next_order = 1;
  uint64_t next_trade = 1;
  std::vector<uint64_t> live_orders;
  live_orders.reserve(4096);

  uint64_t ts = 1'000'000'000ull;
  for (int i = 0; i < cfg_.messages; ++i) {
    const uint64_t seq = static_cast<uint64_t>(i + 1);
    ts += 1'000ull;
    const int roll = type_dist(rng);
    std::vector<std::byte> frame;

    if (roll <= 5 || live_orders.empty()) {
      WireAddOrder msg{};
      const uint64_t oid = next_order++;
      encode_add(msg, seq, ts, oid, sym_dist(rng), (oid & 1) ? Side::Buy : Side::Sell, px_dist(rng),
                 qty_dist(rng));
      frame.resize(sizeof(msg));
      std::memcpy(frame.data(), &msg, sizeof(msg));
      live_orders.push_back(oid);
    } else if (roll <= 7) {
      const uint64_t oid = live_orders[rng() % live_orders.size()];
      WireModifyOrder msg{};
      encode_modify(msg, seq, ts, oid, sym_dist(rng), (oid & 1) ? Side::Buy : Side::Sell,
                    px_dist(rng), qty_dist(rng));
      frame.resize(sizeof(msg));
      std::memcpy(frame.data(), &msg, sizeof(msg));
    } else if (roll == 8 && !live_orders.empty()) {
      const std::size_t idx = static_cast<std::size_t>(rng() % live_orders.size());
      const uint64_t oid = live_orders[idx];
      WireDeleteOrder msg{};
      encode_delete(msg, seq, ts, oid, sym_dist(rng));
      frame.resize(sizeof(msg));
      std::memcpy(frame.data(), &msg, sizeof(msg));
      live_orders.erase(live_orders.begin() + static_cast<std::ptrdiff_t>(idx));
    } else {
      WireTrade msg{};
      encode_trade(msg, seq, ts, next_trade++, sym_dist(rng), Side::Buy, px_dist(rng),
                   qty_dist(rng));
      frame.resize(sizeof(msg));
      std::memcpy(frame.data(), &msg, sizeof(msg));
    }
    session.push_back(std::move(frame));
  }
  return session;
}

int run_recovery_server(uint16_t port, const std::vector<std::vector<std::byte>>& session,
                        std::atomic<bool>& stop) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return 1;
  }
  int yes = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return 1;
  }
  if (listen(fd, 8) < 0) {
    ::close(fd);
    return 1;
  }

  while (!stop.load(std::memory_order_acquire)) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv{0, 200000};
    const int sel = select(fd + 1, &rfds, nullptr, nullptr, &tv);
    if (sel <= 0) {
      continue;
    }
    const int cfd = accept(fd, nullptr, nullptr);
    if (cfd < 0) {
      continue;
    }
    WireGapRequest req{};
    const ssize_t n = ::recv(cfd, &req, sizeof(req), MSG_WAITALL);
    if (n == static_cast<ssize_t>(sizeof(req))) {
      const uint64_t start = load_be64(&req.start_seq);
      const uint64_t end = load_be64(&req.end_seq);
      std::vector<std::byte> payload;
      for (uint64_t s = start; s <= end; ++s) {
        if (s == 0 || s > session.size()) {
          continue;
        }
        const auto& frame = session[static_cast<std::size_t>(s - 1)];
        payload.insert(payload.end(), frame.begin(), frame.end());
      }
      WireGapResponse resp{};
      encode_gap_response(resp, 0, host_now_ns(), start, end,
                          static_cast<uint32_t>(payload.size()));
      ::send(cfd, &resp, sizeof(resp), 0);
      if (!payload.empty()) {
        ::send(cfd, payload.data(), payload.size(), 0);
      }
    }
    ::close(cfd);
  }
  ::close(fd);
  return 0;
}

int FeedSimulator::run() {
  auto session = generate_session();
  std::atomic<bool> stop{false};
  std::thread recovery([&] { run_recovery_server(cfg_.recovery_port, session, stop); });

  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    stop.store(true);
    recovery.join();
    return 1;
  }

  sockaddr_in dest{};
  dest.sin_family = AF_INET;
  dest.sin_port = htons(cfg_.udp_port);
  inet_pton(AF_INET, cfg_.udp_host.c_str(), &dest.sin_addr);

  std::mt19937_64 rng(cfg_.seed + 99);
  std::uniform_real_distribution<double> unit(0.0, 1.0);

  const auto start = std::chrono::steady_clock::now();
  const double interval_ns =
      cfg_.rate_per_sec > 0 ? 1e9 / static_cast<double>(cfg_.rate_per_sec) : 0.0;

  std::vector<std::pair<std::size_t, std::vector<std::byte>>> delayed;

  for (std::size_t i = 0; i < session.size(); ++i) {
    if (cfg_.loss_rate > 0.0 && unit(rng) < cfg_.loss_rate) {
      continue;
    }
    auto frame = session[i];
    if (cfg_.reorder_rate > 0.0 && unit(rng) < cfg_.reorder_rate && i + 1 < session.size()) {
      delayed.emplace_back(i, std::move(frame));
      continue;
    }
    ::sendto(fd, frame.data(), frame.size(), 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    while (!delayed.empty() && delayed.front().first + 2 <= i) {
      auto& d = delayed.front().second;
      ::sendto(fd, d.data(), d.size(), 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
      delayed.erase(delayed.begin());
    }
    if (interval_ns > 0.0) {
      const auto target =
          start + std::chrono::nanoseconds(static_cast<int64_t>(interval_ns * static_cast<double>(i + 1)));
      std::this_thread::sleep_until(target);
    }
  }
  for (auto& d : delayed) {
    ::sendto(fd, d.second.data(), d.second.size(), 0, reinterpret_cast<sockaddr*>(&dest),
             sizeof(dest));
  }

  ::close(fd);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop.store(true);
  recovery.join();
  if (!cfg_.quiet) {
    std::cerr << "simulator sent session messages=" << session.size() << '\n';
  }
  return 0;
}

}  // namespace mdp
