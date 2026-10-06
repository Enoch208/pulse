#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace pulse::sim {

class Rng {
 public:
  explicit Rng(std::uint64_t seed) {
    for (std::uint64_t& word : state_) {
      word = splitmix(seed);
    }
  }

  std::uint64_t next() {
    const std::uint64_t result = std::rotl(state_[1] * 5, 7) * 9;
    const std::uint64_t shifted = state_[1] << 17U;
    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= shifted;
    state_[3] = std::rotl(state_[3], 45);
    return result;
  }

  std::uint64_t below(std::uint64_t bound) {
    const std::uint64_t threshold = (0 - bound) % bound;
    while (true) {
      const std::uint64_t value = next();
      if (value >= threshold) {
        return value % bound;
      }
    }
  }

  bool percent(std::uint64_t chance) { return below(100) < chance; }

 private:
  static std::uint64_t splitmix(std::uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t mixed = state;
    mixed = (mixed ^ (mixed >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    mixed = (mixed ^ (mixed >> 27U)) * 0x94D049BB133111EBULL;
    return mixed ^ (mixed >> 31U);
  }

  std::array<std::uint64_t, 4> state_{};
};

}
