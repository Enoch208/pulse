#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "pulse/wire/codec.hpp"
#include "pulse/wire/messages.hpp"

namespace pulse::wire {

template <typename T>
struct Layout;

template <>
struct Layout<AddOrder> {
  static constexpr std::uint8_t code = 'A';
  static constexpr std::size_t body_size = 2 + 8 + 1 + 8 + 4;
};

template <>
struct Layout<OrderExecuted> {
  static constexpr std::uint8_t code = 'E';
  static constexpr std::size_t body_size = 2 + 8 + 4 + 8;
};

template <>
struct Layout<OrderCancelled> {
  static constexpr std::uint8_t code = 'X';
  static constexpr std::size_t body_size = 2 + 8 + 4;
};

template <>
struct Layout<OrderDeleted> {
  static constexpr std::uint8_t code = 'D';
  static constexpr std::size_t body_size = 2 + 8;
};

template <>
struct Layout<OrderReplaced> {
  static constexpr std::uint8_t code = 'U';
  static constexpr std::size_t body_size = 2 + 8 + 8 + 8 + 4;
};

template <>
struct Layout<BookDigest> {
  static constexpr std::uint8_t code = 'G';
  static constexpr std::size_t body_size = 2 + 8;
};

template <>
struct Layout<EndOfSession> {
  static constexpr std::uint8_t code = 'Z';
  static constexpr std::size_t body_size = 8;
};

static_assert(Layout<OrderReplaced>::body_size == max_body_size);

inline constexpr std::uint8_t buy_code = 'B';
inline constexpr std::uint8_t sell_code = 'S';

constexpr std::uint8_t side_code(Side side) {
  return side == Side::buy ? buy_code : sell_code;
}

constexpr std::optional<Side> side_from_code(std::uint8_t code) {
  if (code == buy_code) {
    return Side::buy;
  }
  if (code == sell_code) {
    return Side::sell;
  }
  return std::nullopt;
}

}
