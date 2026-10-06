#include <string>

#include "pulse/book/order_book.hpp"

namespace pulse::book {

std::optional<std::string> OrderBook::audit() const {
  std::size_t orders_seen = 0;
  for (const Side side : {Side::buy, Side::sell}) {
    std::optional<std::string> problem;
    std::optional<Price> previous;
    ladder(side).for_each_best_first([&](const LevelRef& ref) {
      if (problem) {
        return;
      }
      if (previous && !ladder(side).ranks_below(ref.price, *previous)) {
        problem = "price ladder out of order at " + std::to_string(ref.price);
      } else if (levels_[ref.level].side != side) {
        problem = "level on the wrong side at " + std::to_string(ref.price);
      } else {
        problem = audit_level(ref, orders_seen);
      }
      previous = ref.price;
    });
    if (problem) {
      return problem;
    }
  }
  if (orders_seen != index_.size()) {
    return "index holds " + std::to_string(index_.size()) + " orders, levels hold " +
           std::to_string(orders_seen);
  }
  if (orders_seen != orders_.live()) {
    return "pool holds " + std::to_string(orders_.live()) + " orders, levels hold " +
           std::to_string(orders_seen);
  }
  return std::nullopt;
}

std::optional<std::string> OrderBook::audit_level(const LevelRef& ref,
                                                  std::size_t& orders_seen) const {
  const Level& level = levels_[ref.level];
  const std::string where = " at price " + std::to_string(ref.price);
  if (level.price != ref.price) {
    return "level price differs from its ladder entry" + where;
  }
  std::uint64_t quantity = 0;
  std::uint32_t count = 0;
  Handle previous = null_handle;
  for (Handle order = level.head; order != null_handle; order = orders_[order].next) {
    const Order& entry = orders_[order];
    if (count > orders_.live()) {
      return "cycle in the order queue" + where;
    }
    if (entry.prev != previous) {
      return "broken back link" + where;
    }
    if (entry.level != ref.level) {
      return "order points at another level" + where;
    }
    if (entry.quantity == 0) {
      return "order with zero quantity" + where;
    }
    if (index_.find(entry.id) != order) {
      return "index does not point at order " + std::to_string(entry.id) + where;
    }
    quantity += entry.quantity;
    ++count;
    previous = order;
  }
  if (count == 0) {
    return "empty level left in the ladder" + where;
  }
  if (level.tail != previous) {
    return "tail is not the last order" + where;
  }
  if (count != level.orders || quantity != level.quantity) {
    return "level totals disagree with its orders" + where;
  }
  orders_seen += count;
  return std::nullopt;
}

}
