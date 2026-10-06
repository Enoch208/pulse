#include "pulse/feed/feed_handler.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "../support/reference_book.hpp"
#include "pulse/feed/session.hpp"
#include "pulse/wire/codec.hpp"
#include "pulse/wire/stream_decoder.hpp"

using namespace pulse;
using namespace pulse::feed;

namespace {

constexpr InstrumentId instruments = 4;

struct RecordedSession {
  sim::OrderFlow flow{{2026, instruments}};
  std::vector<wire::Message> messages;

  RecordedSession() {
    publish_session(flow, {50'000, 1'000},
                    [&](const wire::Message& message) { messages.push_back(message); });
  }
};

FeedError replay(FeedHandler& handler, const std::vector<wire::Message>& messages) {
  for (const wire::Message& message : messages) {
    const FeedError error = handler.on_message(message);
    if (error != FeedError::none) {
      return error;
    }
  }
  return FeedError::none;
}

template <typename T>
T* first_of(std::vector<wire::Message>& messages) {
  for (wire::Message& message : messages) {
    if (T* body = std::get_if<T>(&message.body)) {
      return body;
    }
  }
  return nullptr;
}

}

TEST_CASE("a handler rebuilds every book the engine holds") {
  RecordedSession session;
  FeedHandler handler;
  REQUIRE(replay(handler, session.messages) == FeedError::none);

  CHECK(handler.finished());
  CHECK(handler.stats().messages == session.messages.size());
  CHECK(handler.stats().digests_verified >= 50 * instruments);
  REQUIRE(handler.instruments() == instruments);
  for (InstrumentId i = 0; i < instruments; ++i) {
    CHECK(test::snapshot(handler.book(i)) == test::snapshot(session.flow.engine().book(i)));
  }
}

TEST_CASE("the rebuild survives encoding and arbitrary chunking") {
  RecordedSession session;
  std::vector<std::byte> stream;
  for (const wire::Message& message : session.messages) {
    const std::size_t offset = stream.size();
    stream.resize(offset + wire::frame_size(message.body));
    REQUIRE(wire::encode(message, std::span(stream).subspan(offset)) > 0);
  }

  FeedHandler handler;
  wire::StreamDecoder decoder;
  FeedError error = FeedError::none;
  for (std::size_t offset = 0, chunk = 1; offset < stream.size(); offset += chunk, chunk += 97) {
    const std::size_t size = std::min(chunk, stream.size() - offset);
    REQUIRE(decoder.feed(std::span(stream).subspan(offset, size), [&](const wire::Message& m) {
      if (error == FeedError::none) {
        error = handler.on_message(m);
      }
    }) == wire::DecodeStatus::ok);
  }
  CHECK(error == FeedError::none);
  CHECK(handler.finished());
}

TEST_CASE("a missing message is reported as a gap") {
  RecordedSession session;
  session.messages.erase(session.messages.begin() + 100);
  FeedHandler handler;
  CHECK(replay(handler, session.messages) == FeedError::sequence_gap);
  CHECK(handler.last_sequence() == 100);
}

TEST_CASE("an order event the book cannot apply is rejected") {
  RecordedSession session;
  wire::OrderExecuted* execution = first_of<wire::OrderExecuted>(session.messages);
  REQUIRE(execution != nullptr);
  execution->quantity = 1'000'000;
  FeedHandler handler;
  CHECK(replay(handler, session.messages) == FeedError::book_rejected);
  CHECK(handler.last_book_error() == book::BookError::exceeds_quantity);
}

TEST_CASE("a silently wrong price is caught by the next digest") {
  RecordedSession session;
  wire::AddOrder* add = first_of<wire::AddOrder>(session.messages);
  REQUIRE(add != nullptr);
  add->price += 1;
  FeedHandler handler;
  CHECK(replay(handler, session.messages) == FeedError::digest_mismatch);
}

TEST_CASE("the session must end with the right count and nothing after it") {
  RecordedSession session;

  SECTION("wrong count") {
    std::get<wire::EndOfSession>(session.messages.back().body).message_count += 1;
    FeedHandler handler;
    CHECK(replay(handler, session.messages) == FeedError::wrong_message_count);
  }

  SECTION("trailing message") {
    session.messages.push_back(session.messages.back());
    session.messages.back().sequence += 1;
    FeedHandler handler;
    CHECK(replay(handler, session.messages) == FeedError::message_after_end);
  }
}

TEST_CASE("instrument ids beyond the supported range are refused") {
  FeedHandler handler;
  const wire::Message message{1, 0, wire::OrderDeleted{FeedHandler::max_instruments, 1}};
  CHECK(handler.on_message(message) == FeedError::unknown_instrument);
}
