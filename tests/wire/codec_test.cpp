#include "pulse/wire/codec.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "sample_messages.hpp"

using namespace pulse;
using namespace pulse::wire;

namespace {

std::vector<std::byte> encoded(const Message& message) {
  std::vector<std::byte> out(frame_size(message.body));
  REQUIRE(encode(message, out) == out.size());
  return out;
}

}

TEST_CASE("every message type round trips through its frame") {
  for (const Message& message : test::sample_messages()) {
    const std::vector<std::byte> frame = encoded(message);
    CHECK(frame.size() <= max_frame_size);

    Message decoded{};
    const DecodeResult result = decode(frame, decoded);
    CHECK(result.status == DecodeStatus::ok);
    CHECK(result.consumed == frame.size());
    CHECK(decoded == message);
  }
}

TEST_CASE("an add order has a fixed byte layout") {
  const Message message{1, 2, AddOrder{3, 4, Side::buy, 100, 7}};
  const std::vector<std::byte> expected = test::concat({
      test::bytes({0x28, 0x00}),
      test::bytes({'A'}),
      test::bytes({0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}),
      test::bytes({0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}),
      test::bytes({0x03, 0x00}),
      test::bytes({0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}),
      test::bytes({'B'}),
      test::bytes({0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}),
      test::bytes({0x07, 0x00, 0x00, 0x00}),
  });
  CHECK(encoded(message) == expected);
}

TEST_CASE("a truncated frame asks for more bytes") {
  for (const Message& message : test::sample_messages()) {
    const std::vector<std::byte> frame = encoded(message);
    for (std::size_t length = 0; length < frame.size(); ++length) {
      Message decoded{};
      const DecodeResult result = decode(std::span(frame).first(length), decoded);
      CHECK(result.status == DecodeStatus::need_more);
      CHECK(result.consumed == 0);
    }
  }
}

TEST_CASE("malformed frames are rejected without touching the output") {
  const Message original{9, 9, EndOfSession{9}};
  const Message add{1, 2, AddOrder{3, 4, Side::sell, 5, 6}};

  SECTION("unknown type") {
    std::vector<std::byte> frame = encoded(add);
    frame[2] = std::byte{'?'};
    Message decoded = original;
    CHECK(decode(frame, decoded).status == DecodeStatus::unknown_type);
    CHECK(decoded == original);
  }

  SECTION("length that does not match the type") {
    std::vector<std::byte> frame = encoded(add);
    frame[0] = std::byte{0x27};
    Message decoded = original;
    CHECK(decode(frame, decoded).status == DecodeStatus::bad_length);
    CHECK(decoded == original);
  }

  SECTION("side that is neither buy nor sell") {
    std::vector<std::byte> frame = encoded(add);
    frame[29] = std::byte{'Q'};
    Message decoded = original;
    CHECK(decode(frame, decoded).status == DecodeStatus::bad_side);
    CHECK(decoded == original);
  }
}

TEST_CASE("encoding into a buffer that is too small writes nothing") {
  const Message message{1, 2, OrderDeleted{3, 4}};
  std::array<std::byte, header_size + 9> buffer{};
  CHECK(encode(message, buffer) == 0);
  CHECK(buffer == std::array<std::byte, header_size + 9>{});
}
