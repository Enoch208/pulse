#pragma once

#include <bit>
#include <cstdint>

#include "pulse/book/order_book.hpp"

namespace pulse::book {

class Hasher {
 public:
  void add(std::uint64_t value) { state_ = std::rotl(state_ + value * prime_2, 31) * prime_1; }

  [[nodiscard]] std::uint64_t value() const {
    std::uint64_t mixed = state_;
    mixed = (mixed ^ (mixed >> 33U)) * prime_2;
    mixed = (mixed ^ (mixed >> 29U)) * prime_3;
    return mixed ^ (mixed >> 32U);
  }

 private:
  static constexpr std::uint64_t prime_1 = 0x9E3779B185EBCA87ULL;
  static constexpr std::uint64_t prime_2 = 0xC2B2AE3D27D4EB4FULL;
  static constexpr std::uint64_t prime_3 = 0x165667B19E3779F9ULL;
  std::uint64_t state_ = prime_3;
};

[[nodiscard]] std::uint64_t digest(const OrderBook& book);

}
