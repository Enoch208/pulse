#include "pulse/feed/feed_handler.hpp"

#include <array>
#include <variant>

#include "pulse/book/digest.hpp"

namespace pulse::feed {
namespace {

constexpr std::size_t expected_orders_per_book = 4096;

}

std::string_view describe(FeedError error) {
  constexpr std::array<std::string_view, 7> names = {
      "no error",
      "sequence gap",
      "unknown instrument",
      "book rejected an event",
      "book digest mismatch",
      "wrong message count at end of session",
      "message after end of session",
  };
  return names[static_cast<std::size_t>(error)];
}

FeedError FeedHandler::on_message(const wire::Message& message) {
  if (finished_) {
    return FeedError::message_after_end;
  }
  if (message.sequence != last_sequence_ + 1) {
    return FeedError::sequence_gap;
  }
  last_sequence_ = message.sequence;
  ++stats_.messages;
  return std::visit([this](const auto& body) { return apply(body); }, message.body);
}

FeedError FeedHandler::apply(const wire::AddOrder& message) {
  book::OrderBook* book = book_for(message.instrument);
  if (!book) {
    return FeedError::unknown_instrument;
  }
  return checked(book->add(message.order_id, message.side, message.price, message.quantity),
                 stats_.adds);
}

FeedError FeedHandler::apply(const wire::OrderExecuted& message) {
  book::OrderBook* book = book_for(message.instrument);
  if (!book) {
    return FeedError::unknown_instrument;
  }
  const FeedError error =
      checked(book->execute(message.order_id, message.quantity), stats_.executions);
  if (error == FeedError::none) {
    stats_.executed_quantity += message.quantity;
  }
  return error;
}

FeedError FeedHandler::apply(const wire::OrderCancelled& message) {
  book::OrderBook* book = book_for(message.instrument);
  if (!book) {
    return FeedError::unknown_instrument;
  }
  return checked(book->cancel(message.order_id, message.quantity), stats_.cancels);
}

FeedError FeedHandler::apply(const wire::OrderDeleted& message) {
  book::OrderBook* book = book_for(message.instrument);
  if (!book) {
    return FeedError::unknown_instrument;
  }
  return checked(book->remove(message.order_id), stats_.deletes);
}

FeedError FeedHandler::apply(const wire::OrderReplaced& message) {
  book::OrderBook* book = book_for(message.instrument);
  if (!book) {
    return FeedError::unknown_instrument;
  }
  return checked(
      book->replace(message.order_id, message.new_order_id, message.price, message.quantity),
      stats_.replaces);
}

FeedError FeedHandler::apply(const wire::BookDigest& message) {
  book::OrderBook* book = book_for(message.instrument);
  if (!book) {
    return FeedError::unknown_instrument;
  }
  if (book::digest(*book) != message.digest) {
    return FeedError::digest_mismatch;
  }
  ++stats_.digests_verified;
  return FeedError::none;
}

FeedError FeedHandler::apply(const wire::EndOfSession& message) {
  finished_ = true;
  return message.message_count == last_sequence_ ? FeedError::none : FeedError::wrong_message_count;
}

book::OrderBook* FeedHandler::book_for(InstrumentId instrument) {
  if (instrument >= max_instruments) {
    return nullptr;
  }
  while (books_.size() <= instrument) {
    books_.emplace_back(expected_orders_per_book);
  }
  return &books_[instrument];
}

FeedError FeedHandler::checked(book::BookError error, std::uint64_t& counter) {
  if (error != book::BookError::none) {
    last_book_error_ = error;
    return FeedError::book_rejected;
  }
  ++counter;
  return FeedError::none;
}

}
