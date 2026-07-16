#pragma once

#include "mdp/book.hpp"
#include "mdp/consumers.hpp"
#include "mdp/instrumentation.hpp"
#include "mdp/parser.hpp"
#include "mdp/protocol.hpp"
#include "mdp/sequence_tracker.hpp"
#include "mdp/spsc_queue.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace mdp {

inline constexpr std::size_t kEventQueueCapacity = 1u << 14;

struct HandlerConfig {
  std::string udp_host{"127.0.0.1"};
  uint16_t udp_port{9000};
  std::string recovery_host{"127.0.0.1"};
  uint16_t recovery_port{9001};
  std::string bbo_path{"bbo.log"};
  std::string tape_path{"tape.log"};
  std::string bars_path{"bars.bin"};
  uint64_t time_bar_ns{1'000'000'000ull};
  uint64_t volume_bar_qty{1000};
  bool print_stats{false};
  int run_seconds{0};
};

class FeedHandler {
 public:
  explicit FeedHandler(HandlerConfig cfg);
  ~FeedHandler();

  FeedHandler(const FeedHandler&) = delete;
  FeedHandler& operator=(const FeedHandler&) = delete;

  int run();
  void request_stop();

  const PipelineStats& stats() const { return stats_; }
  const SequenceStats& seq_stats() const { return tracker_.stats(); }
  BookBuilder& book() { return book_; }

  // Test seam: inject a framed datagram without sockets.
  void process_datagram(std::span<const std::byte> bytes, uint64_t wire_ns);

 private:
  void consumer_loop();
  void try_recover();
  void handle_parsed(const ParsedMessage& msg, uint64_t wire_ns, uint64_t parse_ns);
  bool publish(PipelineEvent&& ev);

  HandlerConfig cfg_;
  SequenceTracker tracker_;
  BookBuilder book_;
  SpscQueue<PipelineEvent, kEventQueueCapacity> queue_;
  PipelineStats stats_;
  std::atomic<bool> stop_{false};
  std::thread consumer_;
  BboPublisher bbo_;
  TradeTapeWriter tape_;
  BarAggregator bars_;
  int udp_fd_{-1};
};

struct SimulatorConfig {
  std::string udp_host{"127.0.0.1"};
  uint16_t udp_port{9000};
  uint16_t recovery_port{9001};
  int symbols{4};
  int messages{10000};
  int rate_per_sec{50000};
  double loss_rate{0.0};
  double reorder_rate{0.0};
  uint64_t seed{1};
  bool quiet{false};
};

class FeedSimulator {
 public:
  explicit FeedSimulator(SimulatorConfig cfg);
  int run();

  // Build a deterministic session into memory for tests and captures.
  std::vector<std::vector<std::byte>> generate_session() const;

 private:
  SimulatorConfig cfg_;
};

int run_recovery_server(uint16_t port, const std::vector<std::vector<std::byte>>& session,
                        std::atomic<bool>& stop);

}  // namespace mdp
