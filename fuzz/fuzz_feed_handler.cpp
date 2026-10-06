#include <cstddef>
#include <cstdint>
#include <span>

#include "pulse/feed/feed_handler.hpp"
#include "pulse/wire/stream_decoder.hpp"

using namespace pulse;

namespace {

void check(bool condition) {
  if (!condition) {
    __builtin_trap();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::span<const std::byte> input(reinterpret_cast<const std::byte*>(data), size);
  feed::FeedHandler handler;
  wire::StreamDecoder decoder;
  Sequence sequence = 0;
  decoder.feed(input, [&](wire::Message message) {
    message.sequence = ++sequence;
    if (handler.on_message(message) != feed::FeedError::none) {
      return;
    }
    for (InstrumentId instrument = 0; instrument < feed::FeedHandler::max_instruments;
         ++instrument) {
      if (const book::OrderBook* book = handler.book(instrument)) {
        check(!book->audit().has_value());
      }
    }
  });
  check(handler.books_built() <= feed::FeedHandler::max_instruments);
  return 0;
}
