#pragma once

// Why: an SPSC ring with cache-line padded indices and power-of-two masking
// removes mutex contention and modulo divides that dominate a guarded queue
// when a single feed thread hands off to one consumer.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace mdp {

template <typename T, std::size_t Capacity>
class SpscQueue {
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
  static_assert(Capacity >= 2, "Capacity must be at least 2");
  static_assert(std::is_nothrow_move_constructible_v<T> || std::is_nothrow_copy_constructible_v<T>);

 public:
  SpscQueue() {
    for (std::size_t i = 0; i < Capacity; ++i) {
      slots_[i].sequence.store(i, std::memory_order_relaxed);
    }
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }

  SpscQueue(const SpscQueue&) = delete;
  SpscQueue& operator=(const SpscQueue&) = delete;

  bool try_push(T&& item) noexcept {
    const std::size_t pos = head_.load(std::memory_order_relaxed);
    Slot& slot = slots_[pos & kMask];
    const std::size_t seq = slot.sequence.load(std::memory_order_acquire);
    const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
    if (dif != 0) {
      return false;
    }
    slot.storage = std::move(item);
    slot.sequence.store(pos + 1, std::memory_order_release);
    head_.store(pos + 1, std::memory_order_relaxed);
    return true;
  }

  bool try_push(const T& item) noexcept {
    T copy = item;
    return try_push(std::move(copy));
  }

  bool try_pop(T& out) noexcept {
    const std::size_t pos = tail_.load(std::memory_order_relaxed);
    Slot& slot = slots_[pos & kMask];
    const std::size_t seq = slot.sequence.load(std::memory_order_acquire);
    const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
    if (dif != 0) {
      return false;
    }
    out = std::move(slot.storage);
    slot.sequence.store(pos + Capacity, std::memory_order_release);
    tail_.store(pos + 1, std::memory_order_relaxed);
    return true;
  }

  std::optional<T> try_pop() noexcept {
    T tmp;
    if (!try_pop(tmp)) {
      return std::nullopt;
    }
    return tmp;
  }

  bool empty() const noexcept {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
  }

  std::size_t size_approx() const noexcept {
    const std::size_t h = head_.load(std::memory_order_acquire);
    const std::size_t t = tail_.load(std::memory_order_acquire);
    return h - t;
  }

 private:
  static constexpr std::size_t kMask = Capacity - 1;

  struct Slot {
    alignas(T) T storage{};
    std::atomic<std::size_t> sequence{0};
  };

  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
  alignas(64) Slot slots_[Capacity];
};

}  // namespace mdp
