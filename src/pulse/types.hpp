#pragma once

#include <cstdint>

namespace pulse {

using OrderId = std::uint64_t;
using InstrumentId = std::uint16_t;
using Price = std::int64_t;
using Quantity = std::uint32_t;
using Sequence = std::uint64_t;
using Nanos = std::uint64_t;

enum class Side : std::uint8_t { buy, sell };

constexpr Side opposite(Side side) {
  return side == Side::buy ? Side::sell : Side::buy;
}

}
