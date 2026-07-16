#include "mdp/book.hpp"

namespace mdp {

BookBuilder::BookBuilder() = default;

bool BookBuilder::valid_price(uint32_t price) const {
  return price >= 1 && price <= kPriceLevels;
}

OrderRecord* BookBuilder::find_order(uint64_t order_id) {
  std::size_t idx = static_cast<std::size_t>(order_id) & (kOrderPool - 1);
  for (std::size_t i = 0; i < kOrderPool; ++i) {
    OrderRecord& rec = orders_[idx];
    if (rec.state == 0) {
      return nullptr;
    }
    if (rec.state == 1 && rec.order_id == order_id) {
      return &rec;
    }
    idx = (idx + 1) & (kOrderPool - 1);
  }
  return nullptr;
}

OrderRecord* BookBuilder::insert_order(uint64_t order_id) {
  if (live_count_ >= kOrderPool) {
    return nullptr;
  }
  OrderRecord* existing = find_order(order_id);
  if (existing) {
    return existing;
  }
  std::size_t idx = static_cast<std::size_t>(order_id) & (kOrderPool - 1);
  for (std::size_t i = 0; i < kOrderPool; ++i) {
    OrderRecord& rec = orders_[idx];
    if (rec.state == 0) {
      rec = OrderRecord{};
      rec.order_id = order_id;
      rec.state = 1;
      ++live_count_;
      return &rec;
    }
    idx = (idx + 1) & (kOrderPool - 1);
  }
  return nullptr;
}

void BookBuilder::erase_order(uint64_t order_id) {
  std::size_t idx = static_cast<std::size_t>(order_id) & (kOrderPool - 1);
  std::size_t found = kOrderPool;
  for (std::size_t i = 0; i < kOrderPool; ++i) {
    OrderRecord& rec = orders_[idx];
    if (rec.state == 0) {
      return;
    }
    if (rec.state == 1 && rec.order_id == order_id) {
      found = idx;
      break;
    }
    idx = (idx + 1) & (kOrderPool - 1);
  }
  if (found == kOrderPool) {
    return;
  }

  // Backshift deletion keeps probe chains contiguous so lookups stay short.
  std::size_t hole = found;
  std::size_t cur = (hole + 1) & (kOrderPool - 1);
  while (orders_[cur].state == 1) {
    const std::size_t home = static_cast<std::size_t>(orders_[cur].order_id) & (kOrderPool - 1);
    const bool can_move =
        (hole < cur) ? (home <= hole || home > cur) : (home <= hole && home > cur);
    if (can_move) {
      orders_[hole] = orders_[cur];
      hole = cur;
    }
    cur = (cur + 1) & (kOrderPool - 1);
    if (cur == found) {
      break;
    }
  }
  orders_[hole] = OrderRecord{};
  if (live_count_ > 0) {
    --live_count_;
  }
}

void BookBuilder::recompute_best(SideBook& side, bool is_bid) {
  if (is_bid) {
    uint32_t p = side.best_price ? side.best_price : kPriceLevels;
    for (; p >= 1; --p) {
      if (side.qty[p] > 0) {
        side.best_price = p;
        return;
      }
      if (p == 1) {
        break;
      }
    }
    side.best_price = 0;
  } else {
    uint32_t p = side.best_price ? side.best_price : 1;
    for (; p <= kPriceLevels; ++p) {
      if (side.qty[p] > 0) {
        side.best_price = p;
        return;
      }
    }
    side.best_price = 0;
  }
}

void BookBuilder::add_level(SideBook& side, uint32_t price, uint32_t qty, bool is_bid) {
  side.qty[price] += qty;
  if (side.best_price == 0) {
    side.best_price = price;
    return;
  }
  if (is_bid) {
    if (price > side.best_price) {
      side.best_price = price;
    }
  } else if (price < side.best_price) {
    side.best_price = price;
  }
}

void BookBuilder::remove_level(SideBook& side, uint32_t price, uint32_t qty, bool is_bid) {
  if (qty >= side.qty[price]) {
    side.qty[price] = 0;
  } else {
    side.qty[price] -= qty;
  }
  if (price == side.best_price && side.qty[price] == 0) {
    recompute_best(side, is_bid);
  }
}

NormalizedTick BookBuilder::make_tick(uint32_t symbol_id, uint64_t seq, uint64_t ts_ns,
                                      MsgType cause) const {
  NormalizedTick t;
  t.symbol_id = symbol_id;
  if (symbol_id < kMaxSymbols) {
    const SymbolBook& b = books_[symbol_id];
    t.bid_price = b.bid.best_price;
    t.bid_qty = b.bid.best_price ? b.bid.qty[b.bid.best_price] : 0;
    t.ask_price = b.ask.best_price;
    t.ask_qty = b.ask.best_price ? b.ask.qty[b.ask.best_price] : 0;
    t.last_price = b.last_price;
    t.last_qty = b.last_qty;
  }
  t.seq = seq;
  t.ts_ns = ts_ns;
  t.cause = cause;
  return t;
}

TopOfBook BookBuilder::top(uint32_t symbol_id) const {
  TopOfBook t;
  t.symbol_id = symbol_id;
  if (symbol_id >= kMaxSymbols) {
    return t;
  }
  const SymbolBook& b = books_[symbol_id];
  t.bid_price = b.bid.best_price;
  t.bid_qty = b.bid.best_price ? b.bid.qty[b.bid.best_price] : 0;
  t.ask_price = b.ask.best_price;
  t.ask_qty = b.ask.best_price ? b.ask.qty[b.ask.best_price] : 0;
  t.valid = t.bid_price > 0 || t.ask_price > 0;
  return t;
}

uint32_t BookBuilder::bid_qty_at(uint32_t symbol_id, uint32_t price) const {
  if (symbol_id >= kMaxSymbols || !valid_price(price)) {
    return 0;
  }
  return books_[symbol_id].bid.qty[price];
}

uint32_t BookBuilder::ask_qty_at(uint32_t symbol_id, uint32_t price) const {
  if (symbol_id >= kMaxSymbols || !valid_price(price)) {
    return 0;
  }
  return books_[symbol_id].ask.qty[price];
}

std::optional<NormalizedTick> BookBuilder::on_add(const AddOrder& m) {
  if (m.symbol_id >= kMaxSymbols || !valid_price(m.price) || m.qty == 0) {
    return std::nullopt;
  }
  OrderRecord* rec = insert_order(m.order_id);
  if (!rec) {
    return std::nullopt;
  }
  if (rec->state == 1 && rec->qty > 0 && rec->order_id == m.order_id) {
    // Replace duplicate id by removing prior resting size.
    SymbolBook& prev = books_[rec->symbol_id];
    SideBook& side = (rec->side == Side::Buy) ? prev.bid : prev.ask;
    remove_level(side, rec->price, rec->qty, rec->side == Side::Buy);
  }
  rec->symbol_id = m.symbol_id;
  rec->side = m.side;
  rec->price = m.price;
  rec->qty = m.qty;
  rec->state = 1;

  SymbolBook& book = books_[m.symbol_id];
  SideBook& side = (m.side == Side::Buy) ? book.bid : book.ask;
  const TopOfBook before = top(m.symbol_id);
  add_level(side, m.price, m.qty, m.side == Side::Buy);
  const TopOfBook after = top(m.symbol_id);
  if (before.bid_price == after.bid_price && before.bid_qty == after.bid_qty &&
      before.ask_price == after.ask_price && before.ask_qty == after.ask_qty) {
    return std::nullopt;
  }
  return make_tick(m.symbol_id, m.hdr.seq, m.hdr.ts_ns, MsgType::AddOrder);
}

std::optional<NormalizedTick> BookBuilder::on_modify(const ModifyOrder& m) {
  OrderRecord* rec = find_order(m.order_id);
  if (!rec || m.symbol_id >= kMaxSymbols || !valid_price(m.price) || m.qty == 0) {
    return std::nullopt;
  }
  SymbolBook& book = books_[rec->symbol_id];
  SideBook& old_side = (rec->side == Side::Buy) ? book.bid : book.ask;
  const TopOfBook before = top(rec->symbol_id);
  remove_level(old_side, rec->price, rec->qty, rec->side == Side::Buy);

  rec->symbol_id = m.symbol_id;
  rec->side = m.side;
  rec->price = m.price;
  rec->qty = m.qty;

  SymbolBook& nbook = books_[m.symbol_id];
  SideBook& new_side = (m.side == Side::Buy) ? nbook.bid : nbook.ask;
  add_level(new_side, m.price, m.qty, m.side == Side::Buy);
  const TopOfBook after = top(m.symbol_id);
  if (before.bid_price == after.bid_price && before.bid_qty == after.bid_qty &&
      before.ask_price == after.ask_price && before.ask_qty == after.ask_qty) {
    return std::nullopt;
  }
  return make_tick(m.symbol_id, m.hdr.seq, m.hdr.ts_ns, MsgType::ModifyOrder);
}

std::optional<NormalizedTick> BookBuilder::on_delete(const DeleteOrder& m) {
  OrderRecord* rec = find_order(m.order_id);
  if (!rec) {
    return std::nullopt;
  }
  SymbolBook& book = books_[rec->symbol_id];
  SideBook& side = (rec->side == Side::Buy) ? book.bid : book.ask;
  const TopOfBook before = top(rec->symbol_id);
  remove_level(side, rec->price, rec->qty, rec->side == Side::Buy);
  const uint32_t symbol_id = rec->symbol_id;
  erase_order(m.order_id);
  const TopOfBook after = top(symbol_id);
  if (before.bid_price == after.bid_price && before.bid_qty == after.bid_qty &&
      before.ask_price == after.ask_price && before.ask_qty == after.ask_qty) {
    return std::nullopt;
  }
  return make_tick(symbol_id, m.hdr.seq, m.hdr.ts_ns, MsgType::DeleteOrder);
}

std::optional<NormalizedTick> BookBuilder::on_trade(const Trade& m) {
  if (m.symbol_id >= kMaxSymbols || !valid_price(m.price) || m.qty == 0) {
    return std::nullopt;
  }
  SymbolBook& book = books_[m.symbol_id];
  book.last_price = m.price;
  book.last_qty = m.qty;
  return make_tick(m.symbol_id, m.hdr.seq, m.hdr.ts_ns, MsgType::Trade);
}

}  // namespace mdp
