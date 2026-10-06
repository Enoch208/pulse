#include <format>
#include <span>

#include "harness.hpp"
#include "pulse/wire/codec.hpp"
#include "pulse/wire/stream_decoder.hpp"

using namespace pulse;

namespace {

constexpr std::size_t read_size = 64 * 1024;

std::vector<std::byte> encode_all(const std::vector<wire::Message>& messages) {
  std::vector<std::byte> stream;
  for (const wire::Message& message : messages) {
    const std::size_t offset = stream.size();
    stream.resize(offset + wire::frame_size(message.body));
    if (wire::encode(message, std::span(stream).subspan(offset)) == 0) {
      throw std::runtime_error("encode failed");
    }
  }
  return stream;
}

}

int main() {
  const std::vector<wire::Message> messages = bench::recorded_session(2'000'000);
  const std::vector<std::byte> stream = encode_all(messages);

  Sequence checksum = 0;
  const double seconds = bench::median_seconds(5, [&] {
    wire::StreamDecoder decoder;
    const std::span<const std::byte> bytes(stream);
    for (std::size_t offset = 0; offset < bytes.size(); offset += read_size) {
      decoder.feed(bytes.subspan(offset, std::min(read_size, bytes.size() - offset)),
                   [&](const wire::Message& message) { checksum += message.sequence; });
    }
  });
  if (checksum == 0) {
    throw std::runtime_error("nothing decoded");
  }
  bench::print(std::format(
      "stream decode, {} frames in {} KiB reads, median of 5 runs\n"
      "  throughput   {:.2f} M msg/s, {:.0f} MB/s\n"
      "  mean         {:.1f} ns/msg\n",
      messages.size(), read_size / 1024, static_cast<double>(messages.size()) / seconds / 1e6,
      static_cast<double>(stream.size()) / seconds / 1e6,
      seconds * 1e9 / static_cast<double>(messages.size())));
}
