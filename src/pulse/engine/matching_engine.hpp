#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "pulse/book/order_book.hpp"
#include "pulse/engine/requests.hpp"
#include "pulse/wire/messages.hpp"

namespace pulse::engine {

class MatchingEngine {
 public:
  MatchingEngine(std::size_t instruments, std::size_t expected_orders_per_book);

  Reject submit(const NewOrder& order, std::vector<wire::Body>& events);
  Reject cancel(const CancelOrder& request, std::vector<wire::Body>& events);
  Reject replace(const ReplaceOrder& request, std::vector<wire::Body>& events);

  [[nodiscard]] const book::OrderBook& book(InstrumentId instrument) const {
    return books_[instrument];
  }
  [[nodiscard]] std::size_t instruments() const { return books_.size(); }

 private:
  [[nodiscard]] bool crosses(InstrumentId instrument, Side side, Price limit) const;
  void match(const NewOrder& order, Quantity& remaining, std::vector<wire::Body>& events);

  std::vector<book::OrderBook> books_;
  std::uint64_t last_match_id_ = 0;
};

}
