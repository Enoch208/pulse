#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "pulse/book/order_book.hpp"
#include "pulse/engine/requests.hpp"
#include "pulse/wire/messages.hpp"

namespace pulse::test {

class ReferenceEngine {
 public:
  explicit ReferenceEngine(std::size_t instruments) : books_(instruments) {}

  engine::Reject submit(const engine::NewOrder& order, std::vector<wire::Body>& events) {
    if (order.instrument >= books_.size()) {
      return engine::Reject::unknown_instrument;
    }
    if (order.id == 0 || order.quantity == 0) {
      return engine::Reject::invalid_order;
    }
    std::vector<Resting>& book = books_[order.instrument];
    if (locate(book, order.id) != book.end()) {
      return engine::Reject::duplicate_order;
    }
    Quantity remaining = order.quantity;
    while (remaining > 0) {
      const auto best = best_against(book, order.side, order.price);
      if (best == book.end()) {
        break;
      }
      const Quantity fill = std::min(remaining, best->quantity);
      events.emplace_back(wire::OrderExecuted{order.instrument, best->id, fill, ++match_id_});
      remaining -= fill;
      best->quantity -= fill;
      if (best->quantity == 0) {
        book.erase(best);
      }
    }
    if (remaining > 0) {
      book.push_back({order.id, order.side, order.price, remaining, ++arrival_});
      events.emplace_back(
          wire::AddOrder{order.instrument, order.id, order.side, order.price, remaining});
    }
    return engine::Reject::none;
  }

  engine::Reject cancel(const engine::CancelOrder& request, std::vector<wire::Body>& events) {
    if (request.instrument >= books_.size()) {
      return engine::Reject::unknown_instrument;
    }
    if (request.quantity == 0) {
      return engine::Reject::invalid_order;
    }
    std::vector<Resting>& book = books_[request.instrument];
    const auto order = locate(book, request.id);
    if (order == book.end()) {
      return engine::Reject::unknown_order;
    }
    if (request.quantity >= order->quantity) {
      book.erase(order);
      events.emplace_back(wire::OrderDeleted{request.instrument, request.id});
    } else {
      order->quantity -= request.quantity;
      events.emplace_back(wire::OrderCancelled{request.instrument, request.id, request.quantity});
    }
    return engine::Reject::none;
  }

  engine::Reject replace(const engine::ReplaceOrder& request, std::vector<wire::Body>& events) {
    if (request.instrument >= books_.size()) {
      return engine::Reject::unknown_instrument;
    }
    if (request.new_id == 0 || request.quantity == 0) {
      return engine::Reject::invalid_order;
    }
    std::vector<Resting>& book = books_[request.instrument];
    const auto order = locate(book, request.id);
    if (order == book.end()) {
      return engine::Reject::unknown_order;
    }
    if (request.new_id != request.id && locate(book, request.new_id) != book.end()) {
      return engine::Reject::duplicate_order;
    }
    const Side side = order->side;
    book.erase(order);
    if (best_against(book, side, request.price) != book.end()) {
      events.emplace_back(wire::OrderDeleted{request.instrument, request.id});
      return submit({request.instrument, request.new_id, side, request.price, request.quantity},
                    events);
    }
    book.push_back({request.new_id, side, request.price, request.quantity, ++arrival_});
    events.emplace_back(wire::OrderReplaced{request.instrument, request.id, request.new_id,
                                            request.price, request.quantity});
    return engine::Reject::none;
  }

  [[nodiscard]] std::vector<book::OrderView> snapshot(InstrumentId instrument) const {
    std::vector<Resting> book = books_[instrument];
    std::ranges::sort(book, [](const Resting& a, const Resting& b) {
      if (a.side != b.side) {
        return a.side == Side::buy;
      }
      if (a.price != b.price) {
        return a.side == Side::buy ? a.price > b.price : a.price < b.price;
      }
      return a.arrival < b.arrival;
    });
    std::vector<book::OrderView> out;
    for (const Resting& order : book) {
      out.push_back({order.id, order.side, order.price, order.quantity});
    }
    return out;
  }

 private:
  struct Resting {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
    std::uint64_t arrival;
  };

  static std::vector<Resting>::iterator locate(std::vector<Resting>& book, OrderId id) {
    return std::ranges::find(book, id, &Resting::id);
  }

  static std::vector<Resting>::iterator best_against(std::vector<Resting>& book, Side side,
                                                     Price limit) {
    auto best = book.end();
    for (auto it = book.begin(); it != book.end(); ++it) {
      const bool tradable =
          it->side != side && (side == Side::buy ? it->price <= limit : it->price >= limit);
      if (!tradable) {
        continue;
      }
      const bool better_price = best == book.end() || (side == Side::buy ? it->price < best->price
                                                                         : it->price > best->price);
      const bool same_price_earlier =
          best != book.end() && it->price == best->price && it->arrival < best->arrival;
      if (better_price || same_price_earlier) {
        best = it;
      }
    }
    return best;
  }

  std::vector<std::vector<Resting>> books_;
  std::uint64_t match_id_ = 0;
  std::uint64_t arrival_ = 0;
};

}
