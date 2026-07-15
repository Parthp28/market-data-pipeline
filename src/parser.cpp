#include "mdp/parser.hpp"

#include "mdp/bytes.hpp"

#include <cstring>
#include <type_traits>

namespace mdp {
namespace {

ParseStatus decode_side(uint8_t raw, Side& out) {
  if (raw == static_cast<uint8_t>(Side::Buy) || raw == static_cast<uint8_t>(Side::Sell)) {
    out = static_cast<Side>(raw);
    return ParseStatus::Ok;
  }
  return ParseStatus::BadSide;
}

}  // namespace

ParsedMessage parse_message(std::span<const std::byte> bytes) {
  ParsedMessage out;
  if (bytes.size() < sizeof(WireHeader)) {
    out.status = ParseStatus::Truncated;
    return out;
  }

  WireHeader wh{};
  std::memcpy(&wh, bytes.data(), sizeof(wh));
  const Header hdr = decode_header(wh);
  if (hdr.length < sizeof(WireHeader) || hdr.length > kMaxMessageSize) {
    out.status = ParseStatus::BadLength;
    return out;
  }
  if (bytes.size() < hdr.length) {
    out.status = ParseStatus::Truncated;
    return out;
  }

  const auto expected = wire_length_for(hdr.type);
  if (expected == 0) {
    out.status = ParseStatus::UnknownType;
    return out;
  }
  if (hdr.length != expected) {
    out.status = ParseStatus::BadLength;
    return out;
  }

  out.consumed = hdr.length;

  switch (hdr.type) {
    case MsgType::AddOrder: {
      WireAddOrder w{};
      std::memcpy(&w, bytes.data(), sizeof(w));
      AddOrder m;
      m.hdr = hdr;
      m.order_id = load_be64(&w.order_id);
      m.symbol_id = load_be32(&w.symbol_id);
      if (decode_side(w.side, m.side) != ParseStatus::Ok) {
        out.status = ParseStatus::BadSide;
        return out;
      }
      m.price = load_be32(&w.price);
      m.qty = load_be32(&w.qty);
      out.payload = m;
      out.status = ParseStatus::Ok;
      return out;
    }
    case MsgType::ModifyOrder: {
      WireModifyOrder w{};
      std::memcpy(&w, bytes.data(), sizeof(w));
      ModifyOrder m;
      m.hdr = hdr;
      m.order_id = load_be64(&w.order_id);
      m.symbol_id = load_be32(&w.symbol_id);
      if (decode_side(w.side, m.side) != ParseStatus::Ok) {
        out.status = ParseStatus::BadSide;
        return out;
      }
      m.price = load_be32(&w.price);
      m.qty = load_be32(&w.qty);
      out.payload = m;
      out.status = ParseStatus::Ok;
      return out;
    }
    case MsgType::DeleteOrder: {
      WireDeleteOrder w{};
      std::memcpy(&w, bytes.data(), sizeof(w));
      DeleteOrder m;
      m.hdr = hdr;
      m.order_id = load_be64(&w.order_id);
      m.symbol_id = load_be32(&w.symbol_id);
      out.payload = m;
      out.status = ParseStatus::Ok;
      return out;
    }
    case MsgType::Trade: {
      WireTrade w{};
      std::memcpy(&w, bytes.data(), sizeof(w));
      Trade m;
      m.hdr = hdr;
      m.trade_id = load_be64(&w.trade_id);
      m.symbol_id = load_be32(&w.symbol_id);
      if (decode_side(w.side, m.side) != ParseStatus::Ok) {
        out.status = ParseStatus::BadSide;
        return out;
      }
      m.price = load_be32(&w.price);
      m.qty = load_be32(&w.qty);
      out.payload = m;
      out.status = ParseStatus::Ok;
      return out;
    }
    case MsgType::Heartbeat: {
      WireHeartbeat w{};
      std::memcpy(&w, bytes.data(), sizeof(w));
      Heartbeat m;
      m.hdr = hdr;
      m.exchange_ts_ns = load_be64(&w.exchange_ts_ns);
      out.payload = m;
      out.status = ParseStatus::Ok;
      return out;
    }
    case MsgType::GapRequest: {
      WireGapRequest w{};
      std::memcpy(&w, bytes.data(), sizeof(w));
      GapRequest m;
      m.hdr = hdr;
      m.start_seq = load_be64(&w.start_seq);
      m.end_seq = load_be64(&w.end_seq);
      out.payload = m;
      out.status = ParseStatus::Ok;
      return out;
    }
    case MsgType::GapResponse: {
      WireGapResponse w{};
      std::memcpy(&w, bytes.data(), sizeof(w));
      GapResponse m;
      m.hdr = hdr;
      m.start_seq = load_be64(&w.start_seq);
      m.end_seq = load_be64(&w.end_seq);
      m.payload_bytes = load_be32(&w.payload_bytes);
      out.payload = m;
      out.status = ParseStatus::Ok;
      return out;
    }
  }

  out.status = ParseStatus::UnknownType;
  return out;
}

}  // namespace mdp
