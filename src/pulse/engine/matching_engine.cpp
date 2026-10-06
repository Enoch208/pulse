#include "pulse/engine/matching_engine.hpp"

#include <algorithm>

namespace pulse::engine {

MatchingEngine::MatchingEngine(std::size_t instruments, std::size_t expected_orders_per_book) {
  books_.reserve(instruments);
  for (std::size_t i = 0; i < instruments; ++i) {
    books_.emplace_back(expected_orders_per_book);
  }
}

Reject MatchingEngine::submit(const NewOrder& order, std::vector<wire::Body>& events) {
  if (order.instrument >= books_.size()) {
    return Reject::unknown_instrument;
  }
  if (order.id == 0 || order.quantity == 0) {
    return Reject::invalid_order;
  }
  book::OrderBook& book = books_[order.instrument];
  if (book.find(order.id)) {
    return Reject::duplicate_order;
  }
  Quantity remaining = order.quantity;
  match(order, remaining, events);
  if (remaining > 0) {
    book.add(order.id, order.side, order.price, remaining);
    events.emplace_back(
        wire::AddOrder{order.instrument, order.id, order.side, order.price, remaining});
  }
  return Reject::none;
}

Reject MatchingEngine::cancel(const CancelOrder& request, std::vector<wire::Body>& events) {
  if (request.instrument >= books_.size()) {
    return Reject::unknown_instrument;
  }
  if (request.quantity == 0) {
    return Reject::invalid_order;
  }
  book::OrderBook& book = books_[request.instrument];
  const std::optional<book::OrderView> order = book.find(request.id);
  if (!order) {
    return Reject::unknown_order;
  }
  if (request.quantity >= order->quantity) {
    book.remove(request.id);
    events.emplace_back(wire::OrderDeleted{request.instrument, request.id});
  } else {
    book.cancel(request.id, request.quantity);
    events.emplace_back(wire::OrderCancelled{request.instrument, request.id, request.quantity});
  }
  return Reject::none;
}

Reject MatchingEngine::replace(const ReplaceOrder& request, std::vector<wire::Body>& events) {
  if (request.instrument >= books_.size()) {
    return Reject::unknown_instrument;
  }
  if (request.new_id == 0 || request.quantity == 0) {
    return Reject::invalid_order;
  }
  book::OrderBook& book = books_[request.instrument];
  const std::optional<book::OrderView> order = book.find(request.id);
  if (!order) {
    return Reject::unknown_order;
  }
  if (request.new_id != request.id && book.find(request.new_id)) {
    return Reject::duplicate_order;
  }
  if (crosses(request.instrument, order->side, request.price)) {
    book.remove(request.id);
    events.emplace_back(wire::OrderDeleted{request.instrument, request.id});
    return submit(
        NewOrder{request.instrument, request.new_id, order->side, request.price, request.quantity},
        events);
  }
  book.replace(request.id, request.new_id, request.price, request.quantity);
  events.emplace_back(wire::OrderReplaced{request.instrument, request.id, request.new_id,
                                          request.price, request.quantity});
  return Reject::none;
}

bool MatchingEngine::crosses(InstrumentId instrument, Side side, Price limit) const {
  const std::optional<book::LevelView> contra = books_[instrument].best(opposite(side));
  if (!contra) {
    return false;
  }
  return side == Side::buy ? contra->price <= limit : contra->price >= limit;
}

void MatchingEngine::match(const NewOrder& order, Quantity& remaining,
                           std::vector<wire::Body>& events) {
  book::OrderBook& book = books_[order.instrument];
  while (remaining > 0 && crosses(order.instrument, order.side, order.price)) {
    const book::OrderView resting = *book.front(opposite(order.side));
    const Quantity fill = std::min(remaining, resting.quantity);
    book.execute(resting.id, fill);
    events.emplace_back(wire::OrderExecuted{order.instrument, resting.id, fill, ++last_match_id_});
    remaining -= fill;
  }
}

}
