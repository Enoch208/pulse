#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "pulse/book/order_book.hpp"
#include "pulse/wire/messages.hpp"

namespace pulse::feed {

enum class FeedError : std::uint8_t {
  none,
  sequence_gap,
  unknown_instrument,
  book_rejected,
  digest_mismatch,
  wrong_message_count,
  message_after_end,
};

[[nodiscard]] std::string_view describe(FeedError error);

struct FeedStats {
  std::uint64_t messages = 0;
  std::uint64_t adds = 0;
  std::uint64_t executions = 0;
  std::uint64_t cancels = 0;
  std::uint64_t deletes = 0;
  std::uint64_t replaces = 0;
  std::uint64_t digests_verified = 0;
  std::uint64_t executed_quantity = 0;
};

class FeedHandler {
 public:
  static constexpr std::size_t max_instruments = 1024;

  FeedError on_message(const wire::Message& message);

  [[nodiscard]] bool finished() const { return finished_; }
  [[nodiscard]] const FeedStats& stats() const { return stats_; }
  [[nodiscard]] Sequence last_sequence() const { return last_sequence_; }
  [[nodiscard]] book::BookError last_book_error() const { return last_book_error_; }
  [[nodiscard]] std::size_t books_built() const { return books_built_; }
  [[nodiscard]] const book::OrderBook* book(InstrumentId instrument) const {
    return instrument < books_.size() ? books_[instrument].get() : nullptr;
  }

 private:
  FeedError apply(const wire::AddOrder& message);
  FeedError apply(const wire::OrderExecuted& message);
  FeedError apply(const wire::OrderCancelled& message);
  FeedError apply(const wire::OrderDeleted& message);
  FeedError apply(const wire::OrderReplaced& message);
  FeedError apply(const wire::BookDigest& message);
  FeedError apply(const wire::EndOfSession& message);

  book::OrderBook* book_for(InstrumentId instrument);
  FeedError checked(book::BookError error, std::uint64_t& counter);

  std::vector<std::unique_ptr<book::OrderBook>> books_;
  std::size_t books_built_ = 0;
  FeedStats stats_;
  Sequence last_sequence_ = 0;
  book::BookError last_book_error_ = book::BookError::none;
  bool finished_ = false;
};

}
