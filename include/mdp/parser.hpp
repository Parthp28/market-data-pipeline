#pragma once

// Why: returning a discriminated ParsedMessage by value keeps ownership on the
// stack and lets the hot path reject bad frames with an error code instead of
// exceptions that would unwind through the receive loop.

#include "mdp/protocol.hpp"

#include <cstdint>
#include <span>
#include <type_traits>
#include <variant>

namespace mdp {

enum class ParseStatus : uint8_t {
  Ok = 0,
  Truncated,
  BadLength,
  UnknownType,
  BadSide,
};

using ParsedPayload =
    std::variant<std::monostate, AddOrder, ModifyOrder, DeleteOrder, Trade, Heartbeat, GapRequest,
                 GapResponse>;

struct ParsedMessage {
  ParseStatus status{ParseStatus::Truncated};
  ParsedPayload payload{};
  std::size_t consumed{0};
};

ParsedMessage parse_message(std::span<const std::byte> bytes);

inline const Header* message_header(const ParsedMessage& m) {
  return std::visit(
      [](const auto& v) -> const Header* {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return nullptr;
        } else {
          return &v.hdr;
        }
      },
      m.payload);
}

}  // namespace mdp
