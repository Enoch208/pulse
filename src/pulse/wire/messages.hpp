#pragma once

#include <cstdint>
#include <variant>

#include "pulse/types.hpp"

namespace pulse::wire {

struct AddOrder {
  InstrumentId instrument;
  OrderId order_id;
  Side side;
  Price price;
  Quantity quantity;
  friend bool operator==(const AddOrder&, const AddOrder&) = default;
};

struct OrderExecuted {
  InstrumentId instrument;
  OrderId order_id;
  Quantity quantity;
  std::uint64_t match_id;
  friend bool operator==(const OrderExecuted&, const OrderExecuted&) = default;
};

struct OrderCancelled {
  InstrumentId instrument;
  OrderId order_id;
  Quantity quantity;
  friend bool operator==(const OrderCancelled&, const OrderCancelled&) = default;
};

struct OrderDeleted {
  InstrumentId instrument;
  OrderId order_id;
  friend bool operator==(const OrderDeleted&, const OrderDeleted&) = default;
};

struct OrderReplaced {
  InstrumentId instrument;
  OrderId order_id;
  OrderId new_order_id;
  Price price;
  Quantity quantity;
  friend bool operator==(const OrderReplaced&, const OrderReplaced&) = default;
};

struct BookDigest {
  InstrumentId instrument;
  std::uint64_t digest;
  friend bool operator==(const BookDigest&, const BookDigest&) = default;
};

struct EndOfSession {
  std::uint64_t message_count;
  friend bool operator==(const EndOfSession&, const EndOfSession&) = default;
};

using Body = std::variant<AddOrder, OrderExecuted, OrderCancelled, OrderDeleted, OrderReplaced,
                          BookDigest, EndOfSession>;

struct Message {
  Sequence sequence;
  Nanos timestamp;
  Body body;
  friend bool operator==(const Message&, const Message&) = default;
};

}
