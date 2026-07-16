#pragma once

// Why: a flat price-level array indexed from a fixed tick origin beats a
// tree or hashmap for BBO updates because top-of-book churn stays inside a
// contiguous region the CPU can prefetch without pointer chasing.

#include "mdp/protocol.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace mdp {

inline constexpr uint32_t kMaxSymbols = 32;
inline constexpr uint32_t kPriceLevels = 4096;
inline constexpr std::size_t kOrderPool = 1u << 16;
inline constexpr std::size_t kBitmapWords = (kPriceLevels / 64) + 1;

struct TopOfBook {
  uint32_t symbol_id{0};
  uint32_t bid_price{0};
  uint32_t bid_qty{0};
  uint32_t ask_price{0};
  uint32_t ask_qty{0};
  uint64_t seq{0};
  uint64_t ts_ns{0};
  bool valid{false};
};

struct NormalizedTick {
  uint32_t symbol_id{0};
  uint32_t bid_price{0};
  uint32_t bid_qty{0};
  uint32_t ask_price{0};
  uint32_t ask_qty{0};
  uint32_t last_price{0};
  uint32_t last_qty{0};
  uint64_t seq{0};
  uint64_t ts_ns{0};
  MsgType cause{MsgType::Heartbeat};
};

struct OrderRecord {
  uint64_t order_id{0};
  uint32_t symbol_id{0};
  Side side{Side::Buy};
  uint32_t price{0};
  uint32_t qty{0};
  uint8_t state{0};  // 0 free, 1 live
};

class BookBuilder {
 public:
  BookBuilder();

  std::optional<NormalizedTick> on_add(const AddOrder& m);
  std::optional<NormalizedTick> on_modify(const ModifyOrder& m);
  std::optional<NormalizedTick> on_delete(const DeleteOrder& m);
  std::optional<NormalizedTick> on_trade(const Trade& m);

  TopOfBook top(uint32_t symbol_id) const;
  uint32_t bid_qty_at(uint32_t symbol_id, uint32_t price) const;
  uint32_t ask_qty_at(uint32_t symbol_id, uint32_t price) const;

 private:
  struct SideBook {
    std::array<uint32_t, kPriceLevels + 1> qty{};
    // Why: a word bitmap over ticks turns "find next populated level after
    // deleting the BBO" into a bitscan instead of walking up to 4096 qty slots.
    std::array<uint64_t, kBitmapWords> occupied{};
    uint32_t best_price{0};
  };

  struct SymbolBook {
    SideBook bid{};
    SideBook ask{};
    uint32_t last_price{0};
    uint32_t last_qty{0};
  };

  bool valid_price(uint32_t price) const;
  OrderRecord* find_order(uint64_t order_id);
  OrderRecord* insert_order(uint64_t order_id);
  void erase_order(uint64_t order_id);
  void set_occupied(SideBook& side, uint32_t price, bool on);
  void add_level(SideBook& side, uint32_t price, uint32_t qty, bool is_bid);
  void remove_level(SideBook& side, uint32_t price, uint32_t qty, bool is_bid);
  void recompute_best(SideBook& side, bool is_bid);
  NormalizedTick make_tick(uint32_t symbol_id, uint64_t seq, uint64_t ts_ns, MsgType cause) const;

  std::array<SymbolBook, kMaxSymbols> books_{};
  std::array<OrderRecord, kOrderPool> orders_{};
  std::size_t live_count_{0};
};

}  // namespace mdp
