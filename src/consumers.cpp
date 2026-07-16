#include "mdp/consumers.hpp"

#include "mdp/bytes.hpp"

#include <cstring>

namespace mdp {

BboPublisher::BboPublisher(std::string path) : out_(path, std::ios::out | std::ios::trunc) {}

void BboPublisher::on_tick(const NormalizedTick& tick) {
  if (!out_) {
    return;
  }
  out_ << tick.ts_ns << ',' << tick.symbol_id << ',' << tick.bid_price << ',' << tick.bid_qty << ','
       << tick.ask_price << ',' << tick.ask_qty << ',' << tick.seq << '\n';
  ++count_;
}

TradeTapeWriter::TradeTapeWriter(std::string path)
    : out_(path, std::ios::out | std::ios::trunc) {}

void TradeTapeWriter::on_trade(const TradeEvent& trade) {
  if (!out_) {
    return;
  }
  out_ << trade.ts_ns << ',' << trade.symbol_id << ',' << trade.price << ',' << trade.qty << ','
       << static_cast<unsigned>(trade.side) << ',' << trade.trade_id << ',' << trade.seq << '\n';
  ++count_;
}

BarAggregator::BarAggregator(std::string path, uint64_t time_bar_ns, uint64_t volume_bar_qty)
    : out_(path, std::ios::binary | std::ios::out | std::ios::trunc),
      time_bar_ns_(time_bar_ns),
      volume_bar_qty_(volume_bar_qty) {}

void BarAggregator::write_bar(uint32_t symbol_id, const Acc& a, uint8_t type) {
  BinaryBar bar{};
  bar.symbol_id = symbol_id;
  bar.start_ts_ns = a.start_ts;
  bar.end_ts_ns = a.end_ts;
  bar.open = a.open;
  bar.high = a.high;
  bar.low = a.low;
  bar.close = a.close;
  bar.volume = a.volume;
  bar.bar_type = type;
  std::memset(bar.pad, 0, sizeof(bar.pad));
  out_.write(reinterpret_cast<const char*>(&bar), sizeof(bar));
  if (type == 0) {
    ++time_bars_;
  } else {
    ++volume_bars_;
  }
}

void BarAggregator::on_time(uint32_t symbol_id, const TradeEvent& trade) {
  Acc& a = time_acc_[symbol_id];
  if (!a.active) {
    a.active = true;
    a.start_ts = trade.ts_ns;
    a.end_ts = trade.ts_ns;
    a.open = a.high = a.low = a.close = trade.price;
    a.volume = trade.qty;
    return;
  }
  a.end_ts = trade.ts_ns;
  a.high = trade.price > a.high ? trade.price : a.high;
  a.low = trade.price < a.low ? trade.price : a.low;
  a.close = trade.price;
  a.volume += trade.qty;
  if (trade.ts_ns - a.start_ts >= time_bar_ns_) {
    write_bar(symbol_id, a, 0);
    a = Acc{};
  }
}

void BarAggregator::on_volume(uint32_t symbol_id, const TradeEvent& trade) {
  Acc& a = vol_acc_[symbol_id];
  if (!a.active) {
    a.active = true;
    a.start_ts = trade.ts_ns;
    a.end_ts = trade.ts_ns;
    a.open = a.high = a.low = a.close = trade.price;
    a.volume = trade.qty;
  } else {
    a.end_ts = trade.ts_ns;
    a.high = trade.price > a.high ? trade.price : a.high;
    a.low = trade.price < a.low ? trade.price : a.low;
    a.close = trade.price;
    a.volume += trade.qty;
  }
  while (a.active && a.volume >= volume_bar_qty_) {
    Acc emitted = a;
    emitted.volume = volume_bar_qty_;
    write_bar(symbol_id, emitted, 1);
    const uint64_t leftover = a.volume - volume_bar_qty_;
    if (leftover == 0) {
      a = Acc{};
      break;
    }
    a.volume = leftover;
    a.start_ts = trade.ts_ns;
    a.open = a.high = a.low = a.close = trade.price;
  }
}

void BarAggregator::on_trade(const TradeEvent& trade) {
  if (trade.symbol_id >= kMaxSymbols) {
    return;
  }
  on_time(trade.symbol_id, trade);
  on_volume(trade.symbol_id, trade);
}

void BarAggregator::flush_all() {
  for (uint32_t s = 0; s < kMaxSymbols; ++s) {
    if (time_acc_[s].active) {
      write_bar(s, time_acc_[s], 0);
      time_acc_[s] = Acc{};
    }
    if (vol_acc_[s].active && vol_acc_[s].volume > 0) {
      write_bar(s, vol_acc_[s], 1);
      vol_acc_[s] = Acc{};
    }
  }
  out_.flush();
}

}  // namespace mdp
