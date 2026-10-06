#pragma once

#include <cstdint>

#include "pulse/book/order_book.hpp"

namespace pulse::book {

class Fnv1a {
 public:
  void add(std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
      hash_ ^= (value >> shift) & 0xFFU;
      hash_ *= prime;
    }
  }

  [[nodiscard]] std::uint64_t value() const { return hash_; }

 private:
  static constexpr std::uint64_t prime = 0x100000001B3ULL;
  std::uint64_t hash_ = 0xCBF29CE484222325ULL;
};

[[nodiscard]] std::uint64_t digest(const OrderBook& book);

}
