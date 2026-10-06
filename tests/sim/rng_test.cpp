#include "pulse/sim/rng.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>

using pulse::sim::Rng;

TEST_CASE("the generator matches an independent xoshiro256** implementation") {
  Rng rng(20261006);
  CHECK(rng.next() == 0x2113D1B783F9023FULL);
  CHECK(rng.next() == 0x3D09350CD5A13EA5ULL);
  CHECK(rng.next() == 0x56C31DBF297CE4A2ULL);
}

TEST_CASE("bounded draws stay in range and cover it evenly") {
  Rng rng(1);
  std::array<int, 7> counts{};
  for (int i = 0; i < 70'000; ++i) {
    const std::uint64_t value = rng.below(counts.size());
    REQUIRE(value < counts.size());
    ++counts[value];
  }
  for (const int count : counts) {
    CHECK(count > 9'000);
    CHECK(count < 11'000);
  }
}
