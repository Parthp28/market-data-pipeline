#pragma once

// Why: a fixed reorder window beats a heap map of stray sequences because gap
// bursts are short, the slot count is known at compile time, and lookups stay
// inside a few cache lines on the receive path.

#include "mdp/protocol.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace mdp {

inline constexpr std::size_t kReorderWindow = 64;
inline constexpr std::size_t kMaxStoredMessage = kMaxMessageSize;

struct SequenceStats {
  uint64_t next_expected{1};
  uint64_t drops{0};
  uint64_t recovered{0};
  uint64_t out_of_order{0};
  uint64_t recovery_latency_ns{0};
  uint64_t recovery_events{0};
};

class SequenceTracker {
 public:
  SequenceTracker() = default;

  // Push one framed message. On success, ready() returns in-order frames to drain.
  bool push(std::span<const std::byte> framed);
  void note_recovery(uint64_t latency_ns);

  const std::vector<std::vector<std::byte>>& ready() const { return ready_; }
  void clear_ready() { ready_.clear(); }

  bool gap_pending() const { return gap_start_ != 0; }
  uint64_t gap_start() const { return gap_start_; }
  uint64_t gap_end() const { return gap_end_; }
  void clear_gap() {
    gap_start_ = 0;
    gap_end_ = 0;
  }

  const SequenceStats& stats() const { return stats_; }

 private:
  struct Slot {
    bool occupied{false};
    uint16_t length{0};
    std::array<std::byte, kMaxStoredMessage> data{};
  };

  void flush_contiguous();
  void store_ooo(uint64_t seq, std::span<const std::byte> framed);

  SequenceStats stats_{};
  std::array<Slot, kReorderWindow> window_{};
  std::vector<std::vector<std::byte>> ready_{};
  uint64_t gap_start_{0};
  uint64_t gap_end_{0};
};

}  // namespace mdp
