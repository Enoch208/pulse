#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace pulse::wire {

class ByteWriter {
 public:
  explicit ByteWriter(std::span<std::byte> out) : out_(out) {}

  void put_u8(std::uint8_t value) { put(value); }
  void put_u16(std::uint16_t value) { put(value); }
  void put_u32(std::uint32_t value) { put(value); }
  void put_u64(std::uint64_t value) { put(value); }
  void put_i64(std::int64_t value) { put(static_cast<std::uint64_t>(value)); }

  [[nodiscard]] std::size_t written() const { return offset_; }

 private:
  template <std::unsigned_integral T>
  void put(T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
      out_[offset_ + i] = static_cast<std::byte>((value >> (8U * i)) & 0xFFU);
    }
    offset_ += sizeof(T);
  }

  std::span<std::byte> out_;
  std::size_t offset_ = 0;
};

class ByteReader {
 public:
  explicit ByteReader(std::span<const std::byte> in) : in_(in) {}

  std::uint8_t read_u8() { return read<std::uint8_t>(); }
  std::uint16_t read_u16() { return read<std::uint16_t>(); }
  std::uint32_t read_u32() { return read<std::uint32_t>(); }
  std::uint64_t read_u64() { return read<std::uint64_t>(); }
  std::int64_t read_i64() { return static_cast<std::int64_t>(read<std::uint64_t>()); }

  [[nodiscard]] std::size_t consumed() const { return offset_; }

 private:
  template <std::unsigned_integral T>
  T read() {
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
      const auto byte = std::to_integer<T>(in_[offset_ + i]);
      value = static_cast<T>(value | static_cast<T>(byte << (8U * i)));
    }
    offset_ += sizeof(T);
    return value;
  }

  std::span<const std::byte> in_;
  std::size_t offset_ = 0;
};

}
