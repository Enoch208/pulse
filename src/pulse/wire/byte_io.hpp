#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
    const std::span<std::byte> bytes = out_.subspan(offset_, sizeof(T));
    if constexpr (std::endian::native == std::endian::little) {
      std::memcpy(bytes.data(), &value, sizeof(T));
    } else {
      for (std::size_t i = 0; i < sizeof(T); ++i) {
        bytes[i] = static_cast<std::byte>((value >> (8U * i)) & 0xFFU);
      }
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
    const std::span<const std::byte> bytes = in_.subspan(offset_, sizeof(T));
    T value = 0;
    if constexpr (std::endian::native == std::endian::little) {
      std::memcpy(&value, bytes.data(), sizeof(T));
    } else {
      for (std::size_t i = 0; i < sizeof(T); ++i) {
        value = static_cast<T>(value | static_cast<T>(std::to_integer<T>(bytes[i]) << (8U * i)));
      }
    }
    offset_ += sizeof(T);
    return value;
  }

  std::span<const std::byte> in_;
  std::size_t offset_ = 0;
};

}
