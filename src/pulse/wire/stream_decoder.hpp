#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "pulse/wire/codec.hpp"

namespace pulse::wire {

class StreamDecoder {
 public:
  template <typename Handler>
  DecodeStatus feed(std::span<const std::byte> chunk, Handler&& on_message) {
    chunk = complete_pending(chunk, on_message);
    if (status_ != DecodeStatus::ok || pending_size_ != 0) {
      return status_;
    }
    const std::size_t used = drain(chunk, on_message);
    if (status_ == DecodeStatus::ok) {
      keep_pending(chunk.subspan(used));
    }
    return status_;
  }

  [[nodiscard]] DecodeStatus status() const { return status_; }
  [[nodiscard]] std::uint64_t consumed_bytes() const { return consumed_bytes_; }
  [[nodiscard]] std::size_t buffered_bytes() const { return pending_size_; }

 private:
  template <typename Handler>
  std::span<const std::byte> complete_pending(std::span<const std::byte> chunk,
                                              Handler& on_message) {
    while (status_ == DecodeStatus::ok && pending_size_ != 0 && !chunk.empty()) {
      const std::size_t take = std::min(missing_bytes(), chunk.size());
      std::copy_n(chunk.begin(), take, pending_.begin() + pending_size_);
      pending_size_ += take;
      chunk = chunk.subspan(take);
      if (drain(pending(), on_message) == pending_size_) {
        pending_size_ = 0;
      }
    }
    return chunk;
  }

  template <typename Handler>
  std::size_t drain(std::span<const std::byte> bytes, Handler& on_message) {
    std::size_t offset = 0;
    Message message{};
    while (status_ == DecodeStatus::ok) {
      const DecodeResult result = decode(bytes.subspan(offset), message);
      if (result.status == DecodeStatus::need_more) {
        break;
      }
      if (result.status != DecodeStatus::ok) {
        status_ = result.status;
        break;
      }
      offset += result.consumed;
      consumed_bytes_ += result.consumed;
      on_message(message);
    }
    return offset;
  }

  void keep_pending(std::span<const std::byte> tail) {
    std::copy(tail.begin(), tail.end(), pending_.begin());
    pending_size_ = tail.size();
  }

  [[nodiscard]] std::span<const std::byte> pending() const {
    return std::span(pending_).first(pending_size_);
  }

  [[nodiscard]] std::size_t missing_bytes() const {
    constexpr std::size_t probe = length_prefix_size + 1;
    if (pending_size_ < probe) {
      return probe - pending_size_;
    }
    const std::size_t length = std::to_integer<std::size_t>(pending_[0]) |
                               (std::to_integer<std::size_t>(pending_[1]) << 8U);
    return length_prefix_size + length - pending_size_;
  }

  std::array<std::byte, max_frame_size> pending_{};
  std::size_t pending_size_ = 0;
  DecodeStatus status_ = DecodeStatus::ok;
  std::uint64_t consumed_bytes_ = 0;
};

}
