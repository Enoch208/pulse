#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "pulse/book/order_index.hpp"
#include "pulse/book/price_ladder.hpp"
#include "pulse/book/slab.hpp"
#include "pulse/types.hpp"

namespace pulse::book {

enum class BookError : std::uint8_t {
  none,
  invalid_order_id,
  zero_quantity,
  duplicate_order_id,
  unknown_order_id,
  exceeds_quantity,
};

struct LevelView {
  Price price;
  std::uint64_t quantity;
  std::uint32_t orders;
  friend bool operator==(const LevelView&, const LevelView&) = default;
};

struct OrderView {
  OrderId id;
  Side side;
  Price price;
  Quantity quantity;
  friend bool operator==(const OrderView&, const OrderView&) = default;
};

class OrderBook {
 public:
  explicit OrderBook(std::size_t expected_orders = 4096);

  BookError add(OrderId id, Side side, Price price, Quantity quantity);
  BookError execute(OrderId id, Quantity quantity) { return reduce(id, quantity); }
  BookError cancel(OrderId id, Quantity quantity) { return reduce(id, quantity); }
  BookError remove(OrderId id);
  BookError replace(OrderId id, OrderId new_id, Price price, Quantity quantity);

  [[nodiscard]] std::optional<LevelView> best(Side side) const;
  [[nodiscard]] std::optional<OrderView> front(Side side) const;
  [[nodiscard]] std::optional<OrderView> find(OrderId id) const;
  [[nodiscard]] std::size_t order_count() const { return index_.size(); }
  [[nodiscard]] std::size_t level_count(Side side) const { return ladder(side).size(); }

  [[nodiscard]] std::optional<std::string> audit() const;

  template <typename Visit>
  void for_each_level(Side side, Visit&& visit) const {
    ladder(side).for_each_best_first([&](const LevelRef& ref) { visit(view(ref.level)); });
  }

  template <typename Visit>
  void for_each_order(Side side, Visit&& visit) const {
    ladder(side).for_each_best_first([&](const LevelRef& ref) {
      for (Handle order = levels_[ref.level].head; order != null_handle;
           order = orders_[order].next) {
        visit(view_order(order));
      }
    });
  }

 private:
  struct Order {
    OrderId id;
    Quantity quantity;
    Handle level;
    Handle prev;
    Handle next;
  };

  struct Level {
    Price price;
    std::uint64_t quantity;
    Handle head;
    Handle tail;
    std::uint32_t orders;
    Side side;
  };

  [[nodiscard]] const PriceLadder& ladder(Side side) const;
  [[nodiscard]] PriceLadder& ladder(Side side);
  [[nodiscard]] LevelView view(Handle level) const;
  [[nodiscard]] OrderView view_order(Handle order) const;
  [[nodiscard]] std::optional<std::string> audit_level(const LevelRef& ref,
                                                       std::size_t& orders_seen) const;

  Handle level_for(Side side, Price price);
  void append(Handle level, Handle order);
  void detach(Handle order);
  BookError reduce(OrderId id, Quantity quantity);

  Slab<Order> orders_;
  Slab<Level> levels_;
  OrderIndex index_;
  PriceLadder bids_{Side::buy};
  PriceLadder asks_{Side::sell};
};

}
