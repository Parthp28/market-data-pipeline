#pragma once

// Why: consumers pull from the same SPSC queue as typed events so BBO print,
// trade tape, and bar aggregation stay off the parse path without adding a
// second fan-out queue per sink.

#include "mdp/book.hpp"
#include "mdp/protocol.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <string>

namespace mdp {

enum class EventKind : uint8_t { Tick = 1, TradeEvt = 2 };

struct TradeEvent {
  uint32_t symbol_id{0};
  uint32_t price{0};
  uint32_t qty{0};
  Side side{Side::Buy};
  uint64_t seq{0};
  uint64_t ts_ns{0};
  uint64_t trade_id{0};
};

struct PipelineEvent {
  EventKind kind{EventKind::Tick};
  NormalizedTick tick{};
  TradeEvent trade{};
};

class BboPublisher {
 public:
  explicit BboPublisher(std::string path);
  void on_tick(const NormalizedTick& tick);
  std::size_t count() const { return count_; }

 private:
  std::ofstream out_;
  std::size_t count_{0};
};

class TradeTapeWriter {
 public:
  explicit TradeTapeWriter(std::string path);
  void on_trade(const TradeEvent& trade);
  std::size_t count() const { return count_; }

 private:
  std::ofstream out_;
  std::size_t count_{0};
};

#pragma pack(push, 1)
struct BinaryBar {
  uint32_t symbol_id;
  uint64_t start_ts_ns;
  uint64_t end_ts_ns;
  uint32_t open;
  uint32_t high;
  uint32_t low;
  uint32_t close;
  uint64_t volume;
  uint8_t bar_type;  // 0 time, 1 volume
  uint8_t pad[7];
};
#pragma pack(pop)
static_assert(sizeof(BinaryBar) == 52, "BinaryBar size");

class BarAggregator {
 public:
  BarAggregator(std::string path, uint64_t time_bar_ns, uint64_t volume_bar_qty);

  void on_trade(const TradeEvent& trade);
  void flush_all();
  std::size_t time_bars() const { return time_bars_; }
  std::size_t volume_bars() const { return volume_bars_; }

 private:
  struct Acc {
    bool active{false};
    uint64_t start_ts{0};
    uint64_t end_ts{0};
    uint32_t open{0};
    uint32_t high{0};
    uint32_t low{0};
    uint32_t close{0};
    uint64_t volume{0};
  };

  void write_bar(uint32_t symbol_id, const Acc& a, uint8_t type);
  void on_time(uint32_t symbol_id, const TradeEvent& trade);
  void on_volume(uint32_t symbol_id, const TradeEvent& trade);

  std::ofstream out_;
  uint64_t time_bar_ns_;
  uint64_t volume_bar_qty_;
  std::array<Acc, kMaxSymbols> time_acc_{};
  std::array<Acc, kMaxSymbols> vol_acc_{};
  std::size_t time_bars_{0};
  std::size_t volume_bars_{0};
};

}  // namespace mdp
