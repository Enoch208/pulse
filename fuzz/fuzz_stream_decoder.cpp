#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "pulse/wire/codec.hpp"
#include "pulse/wire/stream_decoder.hpp"

using namespace pulse;

namespace {

struct Decoded {
  std::vector<wire::Message> messages;
  wire::DecodeStatus status = wire::DecodeStatus::ok;
  std::uint64_t consumed = 0;
};

Decoded decode_in_chunks(std::span<const std::byte> input, std::size_t chunk) {
  Decoded result;
  wire::StreamDecoder decoder;
  for (std::size_t offset = 0; offset < input.size(); offset += chunk) {
    result.status = decoder.feed(input.subspan(offset, std::min(chunk, input.size() - offset)),
                                 [&](const wire::Message& m) { result.messages.push_back(m); });
  }
  result.consumed = decoder.consumed_bytes();
  return result;
}

void check(bool condition) {
  if (!condition) {
    __builtin_trap();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size == 0) {
    return 0;
  }
  const std::size_t chunk = data[0] % 64 + 1;
  const std::span<const std::byte> input(reinterpret_cast<const std::byte*>(data + 1), size - 1);

  const Decoded whole = decode_in_chunks(input, input.size() + 1);
  const Decoded split = decode_in_chunks(input, chunk);
  check(whole.messages == split.messages);
  check(whole.status == split.status);
  check(whole.consumed == split.consumed);

  std::vector<std::byte> reencoded;
  for (const wire::Message& message : whole.messages) {
    const std::size_t offset = reencoded.size();
    reencoded.resize(offset + wire::frame_size(message.body));
    check(wire::encode(message, std::span(reencoded).subspan(offset)) > 0);
  }
  check(reencoded.size() == whole.consumed);
  check(std::equal(reencoded.begin(), reencoded.end(), input.begin()));
  return 0;
}
