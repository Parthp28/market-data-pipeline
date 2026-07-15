#include "mdp/sequence_tracker.hpp"

#include "mdp/bytes.hpp"
#include "mdp/parser.hpp"

#include <cstring>

namespace mdp {

bool SequenceTracker::push(std::span<const std::byte> framed) {
  if (framed.size() < sizeof(WireHeader) || framed.size() > kMaxStoredMessage) {
    return false;
  }
  WireHeader wh{};
  std::memcpy(&wh, framed.data(), sizeof(wh));
  const uint64_t seq = load_be64(&wh.seq);

  if (seq == stats_.next_expected) {
    ready_.emplace_back(framed.begin(), framed.end());
    ++stats_.next_expected;
    flush_contiguous();
    return true;
  }

  if (seq < stats_.next_expected) {
    ++stats_.drops;
    return false;
  }

  ++stats_.out_of_order;
  store_ooo(seq, framed);

  if (gap_start_ == 0) {
    gap_start_ = stats_.next_expected;
    gap_end_ = seq - 1;
  } else {
    if (seq - 1 > gap_end_) {
      gap_end_ = seq - 1;
    }
  }
  return true;
}

void SequenceTracker::note_recovery(uint64_t latency_ns) {
  stats_.recovery_latency_ns += latency_ns;
  ++stats_.recovery_events;
}

void SequenceTracker::store_ooo(uint64_t seq, std::span<const std::byte> framed) {
  const std::size_t idx = static_cast<std::size_t>(seq % kReorderWindow);
  Slot& slot = window_[idx];
  if (slot.occupied) {
    WireHeader existing{};
    std::memcpy(&existing, slot.data.data(), sizeof(existing));
    const uint64_t existing_seq = load_be64(&existing.seq);
    if (existing_seq == seq) {
      return;
    }
    ++stats_.drops;
  }
  slot.occupied = true;
  slot.length = static_cast<uint16_t>(framed.size());
  std::memcpy(slot.data.data(), framed.data(), framed.size());
}

void SequenceTracker::flush_contiguous() {
  while (true) {
    const std::size_t idx = static_cast<std::size_t>(stats_.next_expected % kReorderWindow);
    Slot& slot = window_[idx];
    if (!slot.occupied) {
      break;
    }
    WireHeader wh{};
    std::memcpy(&wh, slot.data.data(), sizeof(wh));
    const uint64_t seq = load_be64(&wh.seq);
    if (seq != stats_.next_expected) {
      break;
    }
    ready_.emplace_back(slot.data.begin(), slot.data.begin() + slot.length);
    slot.occupied = false;
    slot.length = 0;
    ++stats_.next_expected;
    ++stats_.recovered;
  }

  if (gap_start_ != 0 && stats_.next_expected > gap_end_) {
    clear_gap();
  }
}

}  // namespace mdp
