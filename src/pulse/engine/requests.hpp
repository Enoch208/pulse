#pragma once

#include <cstdint>

#include "pulse/types.hpp"

namespace pulse::engine {

struct NewOrder {
  InstrumentId instrument;
  OrderId id;
  Side side;
  Price price;
  Quantity quantity;
};

struct CancelOrder {
  InstrumentId instrument;
  OrderId id;
  Quantity quantity;
};

struct ReplaceOrder {
  InstrumentId instrument;
  OrderId id;
  OrderId new_id;
  Price price;
  Quantity quantity;
};

enum class Reject : std::uint8_t {
  none,
  unknown_instrument,
  invalid_order,
  unknown_order,
  duplicate_order,
};

}
