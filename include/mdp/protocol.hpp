#pragma once

// Why: packed fixed-width ITCH-style structs let the parser alias the UDP
// receive buffer with std::span and avoid heap strings or TLV walks that would
// dominate latency under bursty exchange traffic.

#include "mdp/bytes.hpp"

#include <cstdint>
#include <cstring>
#include <span>

namespace mdp {

enum class MsgType : uint8_t {
  AddOrder = 1,
  ModifyOrder = 2,
  DeleteOrder = 3,
  Trade = 4,
  Heartbeat = 5,
  GapRequest = 6,
  GapResponse = 7,
};

enum class Side : uint8_t { Buy = 0, Sell = 1 };

#pragma pack(push, 1)

struct WireHeader {
  uint8_t type;
  uint8_t pad;
  uint16_t length;
  uint64_t seq;
  uint64_t ts_ns;
};
static_assert(sizeof(WireHeader) == 20, "WireHeader size");

struct WireAddOrder {
  WireHeader hdr;
  uint64_t order_id;
  uint32_t symbol_id;
  uint8_t side;
  uint8_t pad[3];
  uint32_t price;
  uint32_t qty;
};
static_assert(sizeof(WireAddOrder) == 44, "WireAddOrder size");

struct WireModifyOrder {
  WireHeader hdr;
  uint64_t order_id;
  uint32_t symbol_id;
  uint8_t side;
  uint8_t pad[3];
  uint32_t price;
  uint32_t qty;
};
static_assert(sizeof(WireModifyOrder) == 44, "WireModifyOrder size");

struct WireDeleteOrder {
  WireHeader hdr;
  uint64_t order_id;
  uint32_t symbol_id;
  uint8_t pad[4];
};
static_assert(sizeof(WireDeleteOrder) == 36, "WireDeleteOrder size");

struct WireTrade {
  WireHeader hdr;
  uint64_t trade_id;
  uint32_t symbol_id;
  uint8_t side;
  uint8_t pad[3];
  uint32_t price;
  uint32_t qty;
};
static_assert(sizeof(WireTrade) == 44, "WireTrade size");

struct WireHeartbeat {
  WireHeader hdr;
  uint64_t exchange_ts_ns;
};
static_assert(sizeof(WireHeartbeat) == 28, "WireHeartbeat size");

struct WireGapRequest {
  WireHeader hdr;
  uint64_t start_seq;
  uint64_t end_seq;
};
static_assert(sizeof(WireGapRequest) == 36, "WireGapRequest size");

struct WireGapResponse {
  WireHeader hdr;
  uint64_t start_seq;
  uint64_t end_seq;
  uint32_t payload_bytes;
  uint32_t pad;
};
static_assert(sizeof(WireGapResponse) == 44, "WireGapResponse size");

#pragma pack(pop)

inline constexpr std::size_t kMaxMessageSize = sizeof(WireAddOrder);
inline constexpr std::size_t kMinMessageSize = sizeof(WireHeader);

struct Header {
  MsgType type{};
  uint16_t length{};
  uint64_t seq{};
  uint64_t ts_ns{};
};

struct AddOrder {
  Header hdr{};
  uint64_t order_id{};
  uint32_t symbol_id{};
  Side side{};
  uint32_t price{};
  uint32_t qty{};
};

struct ModifyOrder {
  Header hdr{};
  uint64_t order_id{};
  uint32_t symbol_id{};
  Side side{};
  uint32_t price{};
  uint32_t qty{};
};

struct DeleteOrder {
  Header hdr{};
  uint64_t order_id{};
  uint32_t symbol_id{};
};

struct Trade {
  Header hdr{};
  uint64_t trade_id{};
  uint32_t symbol_id{};
  Side side{};
  uint32_t price{};
  uint32_t qty{};
};

struct Heartbeat {
  Header hdr{};
  uint64_t exchange_ts_ns{};
};

struct GapRequest {
  Header hdr{};
  uint64_t start_seq{};
  uint64_t end_seq{};
};

struct GapResponse {
  Header hdr{};
  uint64_t start_seq{};
  uint64_t end_seq{};
  uint32_t payload_bytes{};
};

inline uint16_t wire_length_for(MsgType t) {
  switch (t) {
    case MsgType::AddOrder:
      return static_cast<uint16_t>(sizeof(WireAddOrder));
    case MsgType::ModifyOrder:
      return static_cast<uint16_t>(sizeof(WireModifyOrder));
    case MsgType::DeleteOrder:
      return static_cast<uint16_t>(sizeof(WireDeleteOrder));
    case MsgType::Trade:
      return static_cast<uint16_t>(sizeof(WireTrade));
    case MsgType::Heartbeat:
      return static_cast<uint16_t>(sizeof(WireHeartbeat));
    case MsgType::GapRequest:
      return static_cast<uint16_t>(sizeof(WireGapRequest));
    case MsgType::GapResponse:
      return static_cast<uint16_t>(sizeof(WireGapResponse));
  }
  return 0;
}

inline void write_header(WireHeader& wh, MsgType type, uint64_t seq, uint64_t ts_ns) {
  wh.type = static_cast<uint8_t>(type);
  wh.pad = 0;
  store_be16(&wh.length, wire_length_for(type));
  store_be64(&wh.seq, seq);
  store_be64(&wh.ts_ns, ts_ns);
}

inline Header decode_header(const WireHeader& wh) {
  Header h;
  h.type = static_cast<MsgType>(wh.type);
  h.length = load_be16(&wh.length);
  h.seq = load_be64(&wh.seq);
  h.ts_ns = load_be64(&wh.ts_ns);
  return h;
}

inline void encode_add(WireAddOrder& out, uint64_t seq, uint64_t ts_ns, uint64_t order_id,
                       uint32_t symbol_id, Side side, uint32_t price, uint32_t qty) {
  write_header(out.hdr, MsgType::AddOrder, seq, ts_ns);
  store_be64(&out.order_id, order_id);
  store_be32(&out.symbol_id, symbol_id);
  out.side = static_cast<uint8_t>(side);
  std::memset(out.pad, 0, sizeof(out.pad));
  store_be32(&out.price, price);
  store_be32(&out.qty, qty);
}

inline void encode_modify(WireModifyOrder& out, uint64_t seq, uint64_t ts_ns, uint64_t order_id,
                          uint32_t symbol_id, Side side, uint32_t price, uint32_t qty) {
  write_header(out.hdr, MsgType::ModifyOrder, seq, ts_ns);
  store_be64(&out.order_id, order_id);
  store_be32(&out.symbol_id, symbol_id);
  out.side = static_cast<uint8_t>(side);
  std::memset(out.pad, 0, sizeof(out.pad));
  store_be32(&out.price, price);
  store_be32(&out.qty, qty);
}

inline void encode_delete(WireDeleteOrder& out, uint64_t seq, uint64_t ts_ns, uint64_t order_id,
                          uint32_t symbol_id) {
  write_header(out.hdr, MsgType::DeleteOrder, seq, ts_ns);
  store_be64(&out.order_id, order_id);
  store_be32(&out.symbol_id, symbol_id);
  std::memset(out.pad, 0, sizeof(out.pad));
}

inline void encode_trade(WireTrade& out, uint64_t seq, uint64_t ts_ns, uint64_t trade_id,
                         uint32_t symbol_id, Side side, uint32_t price, uint32_t qty) {
  write_header(out.hdr, MsgType::Trade, seq, ts_ns);
  store_be64(&out.trade_id, trade_id);
  store_be32(&out.symbol_id, symbol_id);
  out.side = static_cast<uint8_t>(side);
  std::memset(out.pad, 0, sizeof(out.pad));
  store_be32(&out.price, price);
  store_be32(&out.qty, qty);
}

inline void encode_heartbeat(WireHeartbeat& out, uint64_t seq, uint64_t ts_ns,
                             uint64_t exchange_ts_ns) {
  write_header(out.hdr, MsgType::Heartbeat, seq, ts_ns);
  store_be64(&out.exchange_ts_ns, exchange_ts_ns);
}

inline void encode_gap_request(WireGapRequest& out, uint64_t seq, uint64_t ts_ns, uint64_t start,
                               uint64_t end) {
  write_header(out.hdr, MsgType::GapRequest, seq, ts_ns);
  store_be64(&out.start_seq, start);
  store_be64(&out.end_seq, end);
}

inline void encode_gap_response(WireGapResponse& out, uint64_t seq, uint64_t ts_ns, uint64_t start,
                                uint64_t end, uint32_t payload_bytes) {
  write_header(out.hdr, MsgType::GapResponse, seq, ts_ns);
  store_be64(&out.start_seq, start);
  store_be64(&out.end_seq, end);
  store_be32(&out.payload_bytes, payload_bytes);
  out.pad = 0;
}

inline bool encode_message_bytes(std::span<std::byte> dst, MsgType type, uint64_t seq,
                                 uint64_t ts_ns, const void* body_host, std::size_t body_host_len,
                                 std::size_t* written) {
  (void)body_host;
  (void)body_host_len;
  const uint16_t need = wire_length_for(type);
  if (need == 0 || dst.size() < need) {
    return false;
  }
  std::memset(dst.data(), 0, need);
  auto* hdr = reinterpret_cast<WireHeader*>(dst.data());
  write_header(*hdr, type, seq, ts_ns);
  if (written) {
    *written = need;
  }
  return true;
}

}  // namespace mdp
