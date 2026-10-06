#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "pulse/wire/messages.hpp"

namespace pulse::wire {

inline constexpr std::size_t length_prefix_size = 2;
inline constexpr std::size_t timestamp_offset = length_prefix_size + 1 + 8;
inline constexpr std::size_t header_size = timestamp_offset + 8;
inline constexpr std::size_t max_body_size = 30;
inline constexpr std::size_t max_frame_size = header_size + max_body_size;

enum class DecodeStatus : std::uint8_t { ok, need_more, unknown_type, bad_length, bad_side };

struct DecodeResult {
  DecodeStatus status;
  std::size_t consumed;
};

[[nodiscard]] std::size_t frame_size(const Body& body);

[[nodiscard]] std::size_t encode(const Message& message, std::span<std::byte> out);

[[nodiscard]] DecodeResult decode(std::span<const std::byte> in, Message& out);

void restamp(std::span<std::byte> frame, Nanos timestamp);

}
