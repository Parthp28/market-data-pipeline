#include "mdp/spsc_queue.hpp"

#include <benchmark/benchmark.h>

#include <atomic>
#include <mutex>
#include <queue>
#include <thread>

static void BM_SpscRoundTrip(benchmark::State& state) {
  mdp::SpscQueue<int, 1 << 12> q;
  std::atomic<bool> run{true};
  std::atomic<std::uint64_t> popped{0};
  std::thread cons([&] {
    int v = 0;
    while (run.load(std::memory_order_acquire) || !q.empty()) {
      if (q.try_pop(v)) {
        popped.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });
  for (auto _ : state) {
    int spins = 0;
    while (!q.try_push(1)) {
      if (++spins > 1000000) {
        break;
      }
    }
  }
  run.store(false, std::memory_order_release);
  cons.join();
  benchmark::DoNotOptimize(popped.load());
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SpscRoundTrip)->Iterations(200000);

static void BM_MutexQueueRoundTrip(benchmark::State& state) {
  std::mutex mu;
  std::queue<int> q;
  std::atomic<bool> run{true};
  std::thread cons([&] {
    while (run.load(std::memory_order_acquire)) {
      std::lock_guard<std::mutex> lock(mu);
      if (!q.empty()) {
        q.pop();
      }
    }
    std::lock_guard<std::mutex> lock(mu);
    while (!q.empty()) {
      q.pop();
    }
  });
  for (auto _ : state) {
    for (;;) {
      std::lock_guard<std::mutex> lock(mu);
      if (q.size() < 4096) {
        q.push(1);
        break;
      }
    }
  }
  run.store(false, std::memory_order_release);
  cons.join();
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MutexQueueRoundTrip)->Iterations(200000);
