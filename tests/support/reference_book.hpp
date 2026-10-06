#pragma once

#include <algorithm>
#include <deque>
#include <map>
#include <vector>

#include "pulse/book/order_book.hpp"

namespace pulse::test {

class ReferenceBook {
 public:
  book::BookError add(OrderId id, Side side, Price price, Quantity quantity) {
    if (id == 0) {
      return book::BookError::invalid_order_id;
    }
    if (quantity == 0) {
      return book::BookError::zero_quantity;
    }
    if (locate(id)) {
      return book::BookError::duplicate_order_id;
    }
    queue(side, price).push_back({id, side, price, quantity});
    return book::BookError::none;
  }

  book::BookError reduce(OrderId id, Quantity quantity) {
    if (quantity == 0) {
      return book::BookError::zero_quantity;
    }
    book::OrderView* order = locate(id);
    if (!order) {
      return book::BookError::unknown_order_id;
    }
    if (quantity > order->quantity) {
      return book::BookError::exceeds_quantity;
    }
    order->quantity -= quantity;
    if (order->quantity == 0) {
      erase(id);
    }
    return book::BookError::none;
  }

  book::BookError remove(OrderId id) {
    if (!locate(id)) {
      return book::BookError::unknown_order_id;
    }
    erase(id);
    return book::BookError::none;
  }

  book::BookError replace(OrderId id, OrderId new_id, Price price, Quantity quantity) {
    if (new_id == 0) {
      return book::BookError::invalid_order_id;
    }
    if (quantity == 0) {
      return book::BookError::zero_quantity;
    }
    const book::OrderView* order = locate(id);
    if (!order) {
      return book::BookError::unknown_order_id;
    }
    if (new_id != id && locate(new_id)) {
      return book::BookError::duplicate_order_id;
    }
    const Side side = order->side;
    erase(id);
    return add(new_id, side, price, quantity);
  }

  [[nodiscard]] std::vector<book::OrderView> snapshot() const {
    std::vector<book::OrderView> out;
    for (auto it = bids_.rbegin(); it != bids_.rend(); ++it) {
      out.insert(out.end(), it->second.begin(), it->second.end());
    }
    for (const auto& [price, orders] : asks_) {
      out.insert(out.end(), orders.begin(), orders.end());
    }
    return out;
  }

 private:
  using Levels = std::map<Price, std::deque<book::OrderView>>;

  std::deque<book::OrderView>& queue(Side side, Price price) {
    return (side == Side::buy ? bids_ : asks_)[price];
  }

  book::OrderView* locate(OrderId id) {
    for (Levels* levels : {&bids_, &asks_}) {
      for (auto& [price, orders] : *levels) {
        const auto it = std::ranges::find(orders, id, &book::OrderView::id);
        if (it != orders.end()) {
          return &*it;
        }
      }
    }
    return nullptr;
  }

  void erase(OrderId id) {
    for (Levels* levels : {&bids_, &asks_}) {
      for (auto level = levels->begin(); level != levels->end(); ++level) {
        auto& orders = level->second;
        const auto it = std::ranges::find(orders, id, &book::OrderView::id);
        if (it != orders.end()) {
          orders.erase(it);
          if (orders.empty()) {
            levels->erase(level);
          }
          return;
        }
      }
    }
  }

  Levels bids_;
  Levels asks_;
};

inline std::vector<book::OrderView> snapshot(const book::OrderBook& book) {
  std::vector<book::OrderView> out;
  for (const Side side : {Side::buy, Side::sell}) {
    book.for_each_order(side, [&](const book::OrderView& order) { out.push_back(order); });
  }
  return out;
}

}
