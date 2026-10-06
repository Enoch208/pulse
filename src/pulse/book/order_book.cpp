#include "pulse/book/order_book.hpp"

namespace pulse::book {

OrderBook::OrderBook(std::size_t expected_orders)
    : orders_(expected_orders), levels_(expected_orders / 4 + 16), index_(expected_orders) {}

BookError OrderBook::add(OrderId id, Side side, Price price, Quantity quantity) {
  if (id == 0) {
    return BookError::invalid_order_id;
  }
  if (quantity == 0) {
    return BookError::zero_quantity;
  }
  if (index_.find(id) != null_handle) {
    return BookError::duplicate_order_id;
  }
  const Handle level = level_for(side, price);
  const Handle order = orders_.acquire(Order{id, quantity, level, null_handle, null_handle});
  append(level, order);
  index_.insert(id, order);
  return BookError::none;
}

BookError OrderBook::remove(OrderId id) {
  const Handle order = index_.find(id);
  if (order == null_handle) {
    return BookError::unknown_order_id;
  }
  detach(order);
  return BookError::none;
}

BookError OrderBook::replace(OrderId id, OrderId new_id, Price price, Quantity quantity) {
  if (new_id == 0) {
    return BookError::invalid_order_id;
  }
  if (quantity == 0) {
    return BookError::zero_quantity;
  }
  const Handle order = index_.find(id);
  if (order == null_handle) {
    return BookError::unknown_order_id;
  }
  if (new_id != id && index_.find(new_id) != null_handle) {
    return BookError::duplicate_order_id;
  }
  const Side side = levels_[orders_[order].level].side;
  detach(order);
  return add(new_id, side, price, quantity);
}

std::optional<LevelView> OrderBook::best(Side side) const {
  const std::optional<LevelRef> ref = ladder(side).best();
  if (!ref) {
    return std::nullopt;
  }
  return view(ref->level);
}

std::optional<OrderView> OrderBook::front(Side side) const {
  const std::optional<LevelRef> ref = ladder(side).best();
  if (!ref) {
    return std::nullopt;
  }
  return view_order(levels_[ref->level].head);
}

std::optional<OrderView> OrderBook::find(OrderId id) const {
  const Handle order = index_.find(id);
  if (order == null_handle) {
    return std::nullopt;
  }
  return view_order(order);
}

const PriceLadder& OrderBook::ladder(Side side) const {
  return side == Side::buy ? bids_ : asks_;
}

PriceLadder& OrderBook::ladder(Side side) {
  return side == Side::buy ? bids_ : asks_;
}

LevelView OrderBook::view(Handle level) const {
  const Level& entry = levels_[level];
  return LevelView{entry.price, entry.quantity, entry.orders};
}

OrderView OrderBook::view_order(Handle order) const {
  const Order& entry = orders_[order];
  const Level& level = levels_[entry.level];
  return OrderView{entry.id, level.side, level.price, entry.quantity};
}

Handle OrderBook::level_for(Side side, Price price) {
  PriceLadder& prices = ladder(side);
  const Handle existing = prices.find(price);
  if (existing != null_handle) {
    return existing;
  }
  const Handle level = levels_.acquire(Level{price, 0, null_handle, null_handle, 0, side});
  prices.insert(price, level);
  return level;
}

void OrderBook::append(Handle level, Handle order) {
  Level& entry = levels_[level];
  orders_[order].prev = entry.tail;
  if (entry.tail == null_handle) {
    entry.head = order;
  } else {
    orders_[entry.tail].next = order;
  }
  entry.tail = order;
  entry.quantity += orders_[order].quantity;
  ++entry.orders;
}

void OrderBook::detach(Handle order) {
  const Order entry = orders_[order];
  Level& level = levels_[entry.level];
  if (entry.prev == null_handle) {
    level.head = entry.next;
  } else {
    orders_[entry.prev].next = entry.next;
  }
  if (entry.next == null_handle) {
    level.tail = entry.prev;
  } else {
    orders_[entry.next].prev = entry.prev;
  }
  level.quantity -= entry.quantity;
  if (--level.orders == 0) {
    ladder(level.side).erase(level.price);
    levels_.release(entry.level);
  }
  index_.erase(entry.id);
  orders_.release(order);
}

BookError OrderBook::reduce(OrderId id, Quantity quantity) {
  if (quantity == 0) {
    return BookError::zero_quantity;
  }
  const Handle order = index_.find(id);
  if (order == null_handle) {
    return BookError::unknown_order_id;
  }
  Order& entry = orders_[order];
  if (quantity > entry.quantity) {
    return BookError::exceeds_quantity;
  }
  if (quantity == entry.quantity) {
    detach(order);
    return BookError::none;
  }
  entry.quantity -= quantity;
  levels_[entry.level].quantity -= quantity;
  return BookError::none;
}

}
