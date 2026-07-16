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
#include <utility>
#include <variant>

namespace mdp {

FeedHandler::FeedHandler(HandlerConfig cfg)
    : cfg_(std::move(cfg)),
      bbo_(cfg_.bbo_path),
      tape_(cfg_.tape_path),
      bars_(cfg_.bars_path, cfg_.time_bar_ns, cfg_.volume_bar_qty) {}

FeedHandler::~FeedHandler() {
  request_stop();
  if (consumer_.joinable()) {
    consumer_.join();
  }
  if (udp_fd_ >= 0) {
    ::close(udp_fd_);
    udp_fd_ = -1;
  }
}

void FeedHandler::request_stop() { stop_.store(true, std::memory_order_release); }

bool FeedHandler::publish(PipelineEvent&& ev) {
  for (int i = 0; i < 64; ++i) {
    if (queue_.try_push(std::move(ev))) {
      return true;
    }
  }
  return queue_.try_push(std::move(ev));
}

void FeedHandler::consumer_loop() {
  PipelineEvent ev;
  while (!stop_.load(std::memory_order_acquire) || !queue_.empty()) {
    if (!queue_.try_pop(ev)) {
      std::this_thread::yield();
      continue;
    }
    const uint64_t t0 = now_ns();
    if (ev.kind == EventKind::Tick) {
      bbo_.on_tick(ev.tick);
    } else {
      tape_.on_trade(ev.trade);
      bars_.on_trade(ev.trade);
    }
    stats_.book_to_consumer.record(static_cast<int64_t>(now_ns() - t0));
  }
  bars_.flush_all();
}

void FeedHandler::try_recover() {
  if (!tracker_.gap_pending()) {
    return;
  }
  const uint64_t t0 = now_ns();
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return;
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(cfg_.recovery_port);
  inet_pton(AF_INET, cfg_.recovery_host.c_str(), &addr.sin_addr);
  if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return;
  }

  WireGapRequest req{};
  encode_gap_request(req, 0, now_ns(), tracker_.gap_start(), tracker_.gap_end());
  if (::send(fd, &req, sizeof(req), 0) < 0) {
    ::close(fd);
    return;
  }

  WireGapResponse resp{};
  if (::recv(fd, &resp, sizeof(resp), MSG_WAITALL) != static_cast<ssize_t>(sizeof(resp))) {
    ::close(fd);
    return;
  }
  const uint32_t nbytes = load_be32(&resp.payload_bytes);
  std::vector<std::byte> payload(nbytes);
  std::size_t got = 0;
  while (got < payload.size()) {
    const ssize_t n = ::recv(fd, payload.data() + got, payload.size() - got, 0);
    if (n <= 0) {
      break;
    }
    got += static_cast<std::size_t>(n);
  }
  ::close(fd);

  std::size_t off = 0;
  while (off < got) {
    auto parsed = parse_message(std::span<const std::byte>(payload.data() + off, got - off));
    if (parsed.status != ParseStatus::Ok || parsed.consumed == 0) {
      break;
    }
    tracker_.push(std::span<const std::byte>(payload.data() + off, parsed.consumed));
    off += parsed.consumed;
  }
  tracker_.note_recovery(now_ns() - t0);
  tracker_.clear_gap();
}

void FeedHandler::handle_parsed(const ParsedMessage& msg, uint64_t wire_ns, uint64_t parse_ns) {
  stats_.wire_to_parse.record(static_cast<int64_t>(parse_ns - wire_ns));
  stats_.note_message();
  const uint64_t book_t0 = now_ns();

  std::visit(
      [&](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, AddOrder>) {
          if (auto tick = book_.on_add(payload)) {
            PipelineEvent ev;
            ev.kind = EventKind::Tick;
            ev.tick = *tick;
            publish(std::move(ev));
          }
        } else if constexpr (std::is_same_v<T, ModifyOrder>) {
          if (auto tick = book_.on_modify(payload)) {
            PipelineEvent ev;
            ev.kind = EventKind::Tick;
            ev.tick = *tick;
            publish(std::move(ev));
          }
        } else if constexpr (std::is_same_v<T, DeleteOrder>) {
          if (auto tick = book_.on_delete(payload)) {
            PipelineEvent ev;
            ev.kind = EventKind::Tick;
            ev.tick = *tick;
            publish(std::move(ev));
          }
        } else if constexpr (std::is_same_v<T, Trade>) {
          if (auto tick = book_.on_trade(payload)) {
            PipelineEvent ev;
            ev.kind = EventKind::Tick;
            ev.tick = *tick;
            publish(std::move(ev));
          }
          PipelineEvent te;
          te.kind = EventKind::TradeEvt;
          te.trade.symbol_id = payload.symbol_id;
          te.trade.price = payload.price;
          te.trade.qty = payload.qty;
          te.trade.side = payload.side;
          te.trade.seq = payload.hdr.seq;
          te.trade.ts_ns = payload.hdr.ts_ns;
          te.trade.trade_id = payload.trade_id;
          publish(std::move(te));
        }
      },
      msg.payload);

  stats_.parse_to_book.record(static_cast<int64_t>(now_ns() - book_t0));
}

void FeedHandler::process_datagram(std::span<const std::byte> bytes, uint64_t wire_ns) {
  auto parsed = parse_message(bytes);
  const uint64_t parse_ns = now_ns();
  if (parsed.status != ParseStatus::Ok) {
    return;
  }
  tracker_.push(bytes.subspan(0, parsed.consumed));
  if (tracker_.gap_pending()) {
    try_recover();
  }
  for (const auto& frame : tracker_.ready()) {
    auto ordered = parse_message(frame);
    if (ordered.status == ParseStatus::Ok) {
      handle_parsed(ordered, wire_ns, parse_ns);
    }
  }
  tracker_.clear_ready();
}

int FeedHandler::run() {
  udp_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd_ < 0) {
    return 1;
  }
  int yes = 1;
  setsockopt(udp_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(cfg_.udp_port);
  inet_pton(AF_INET, cfg_.udp_host.c_str(), &addr.sin_addr);
  if (bind(udp_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    return 1;
  }

  stats_.start_ns = now_ns();
  consumer_ = std::thread([this] { consumer_loop(); });

  const auto deadline =
      cfg_.run_seconds > 0
          ? std::chrono::steady_clock::now() + std::chrono::seconds(cfg_.run_seconds)
          : std::chrono::steady_clock::time_point::max();

  alignas(64) std::byte buf[2048];
  while (!stop_.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(udp_fd_, &rfds);
    timeval tv{0, 100000};
    const int sel = select(udp_fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (sel <= 0) {
      continue;
    }
    const ssize_t n = ::recvfrom(udp_fd_, buf, sizeof(buf), 0, nullptr, nullptr);
    if (n <= 0) {
      continue;
    }
    process_datagram(std::span<const std::byte>(buf, static_cast<std::size_t>(n)), now_ns());
  }

  request_stop();
  if (consumer_.joinable()) {
    consumer_.join();
  }
  stats_.end_ns = now_ns();
  if (cfg_.print_stats) {
    std::cout << stats_.format_table();
  }
  return 0;
}

}  // namespace mdp
