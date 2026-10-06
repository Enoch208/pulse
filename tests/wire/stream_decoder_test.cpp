#include "pulse/wire/stream_decoder.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "pulse/wire/codec.hpp"
#include "sample_messages.hpp"

using namespace pulse;
using namespace pulse::wire;

namespace {

std::vector<std::byte> encode_all(const std::vector<Message>& messages) {
  std::vector<std::byte> stream;
  for (const Message& message : messages) {
    const std::size_t offset = stream.size();
    stream.resize(offset + frame_size(message.body));
    REQUIRE(encode(message, std::span(stream).subspan(offset)) > 0);
  }
  return stream;
}

std::vector<Message> decode_in_chunks(std::span<const std::byte> stream,
                                      const std::vector<std::size_t>& chunk_sizes) {
  StreamDecoder decoder;
  std::vector<Message> received;
  std::size_t offset = 0;
  for (std::size_t i = 0; offset < stream.size(); ++i) {
    const std::size_t size = std::min(chunk_sizes[i % chunk_sizes.size()], stream.size() - offset);
    const DecodeStatus status = decoder.feed(stream.subspan(offset, size),
                                             [&](const Message& m) { received.push_back(m); });
    REQUIRE(status == DecodeStatus::ok);
    CHECK(decoder.buffered_bytes() < max_frame_size);
    offset += size;
  }
  CHECK(decoder.buffered_bytes() == 0);
  CHECK(decoder.consumed_bytes() == stream.size());
  return received;
}

std::vector<Message> long_session() {
  std::vector<Message> messages;
  for (std::uint64_t round = 0; round < 200; ++round) {
    for (Message message : test::sample_messages()) {
      message.sequence = messages.size() + 1;
      messages.push_back(message);
    }
  }
  return messages;
}

}

TEST_CASE("frames split at every possible point are reassembled") {
  const std::vector<Message> messages = test::sample_messages();
  const std::vector<std::byte> stream = encode_all(messages);
  for (std::size_t split = 1; split < stream.size(); ++split) {
    CHECK(decode_in_chunks(stream, {split, stream.size()}) == messages);
  }
}

TEST_CASE("a stream delivered one byte at a time is reassembled") {
  const std::vector<Message> messages = test::sample_messages();
  CHECK(decode_in_chunks(encode_all(messages), {1}) == messages);
}

TEST_CASE("irregular chunk sizes across a long session lose nothing") {
  const std::vector<Message> messages = long_session();
  const std::vector<std::byte> stream = encode_all(messages);
  CHECK(decode_in_chunks(stream, {1, 7, 2, 64, 3, 41, 1500, 5, 13, 4096}) == messages);
  CHECK(decode_in_chunks(stream, {stream.size()}) == messages);
}

TEST_CASE("a malformed frame stops the stream at its first byte") {
  const std::vector<Message> messages = test::sample_messages();
  std::vector<std::byte> stream = encode_all(messages);
  const std::size_t good_prefix = frame_size(messages[0].body) + frame_size(messages[1].body);
  stream[good_prefix + 2] = std::byte{'?'};

  StreamDecoder decoder;
  std::vector<Message> received;
  const auto collect = [&](const Message& m) { received.push_back(m); };
  const std::size_t first = good_prefix + 1;

  CHECK(decoder.feed(std::span(stream).first(first), collect) == DecodeStatus::ok);
  CHECK(decoder.feed(std::span(stream).subspan(first), collect) == DecodeStatus::unknown_type);
  CHECK(received.size() == 2);
  CHECK(decoder.consumed_bytes() == good_prefix);

  CHECK(decoder.feed(std::span(stream).first(10), collect) == DecodeStatus::unknown_type);
  CHECK(received.size() == 2);
}
