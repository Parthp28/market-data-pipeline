#include "mdp/parser.hpp"
#include "mdp/protocol.hpp"

#include <benchmark/benchmark.h>

#include <cstring>
#include <vector>

namespace {

std::vector<std::byte> make_add() {
  mdp::WireAddOrder msg{};
  mdp::encode_add(msg, 1, 1, 1, 0, mdp::Side::Buy, 1000, 10);
  std::vector<std::byte> buf(sizeof(msg));
  std::memcpy(buf.data(), &msg, sizeof(msg));
  return buf;
}

}  // namespace

static void BM_ParseAddOrder(benchmark::State& state) {
  const auto buf = make_add();
  for (auto _ : state) {
    auto parsed = mdp::parse_message(buf);
    benchmark::DoNotOptimize(parsed);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ParseAddOrder);
