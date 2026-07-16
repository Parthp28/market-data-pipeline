#include "mdp/book.hpp"
#include "mdp/protocol.hpp"

#include <benchmark/benchmark.h>

#include <memory>

static void BM_BookUpdate(benchmark::State& state) {
  auto book = std::make_unique<mdp::BookBuilder>();
  mdp::AddOrder a{};
  a.symbol_id = 0;
  a.side = mdp::Side::Buy;
  a.qty = 1;
  uint64_t oid = 1;
  constexpr uint64_t kWindow = 4096;
  for (auto _ : state) {
    a.order_id = oid;
    a.hdr.seq = oid;
    a.hdr.ts_ns = oid;
    a.price = static_cast<uint32_t>(1000 + (oid % 50));
    auto tick = book->on_add(a);
    benchmark::DoNotOptimize(tick);
    if (oid > kWindow) {
      mdp::DeleteOrder d{};
      d.order_id = oid - kWindow;
      d.symbol_id = 0;
      d.hdr.seq = oid;
      auto t2 = book->on_delete(d);
      benchmark::DoNotOptimize(t2);
    }
    ++oid;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BookUpdate);
