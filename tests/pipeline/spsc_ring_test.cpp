#include "pulse/pipeline/spsc_ring.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <thread>

#include "pulse/wire/messages.hpp"

using pulse::pipeline::SpscRing;

TEST_CASE("capacity rounds up to a power of two") {
  CHECK(SpscRing<int>(0).capacity() == 2);
  CHECK(SpscRing<int>(5).capacity() == 8);
  CHECK(SpscRing<int>(1024).capacity() == 1024);
}

TEST_CASE("items come out in the order they went in, across wraparound") {
  SpscRing<int> ring(4);
  int next_in = 0;
  int next_out = 0;
  for (int round = 0; round < 100; ++round) {
    while (ring.try_push(next_in)) {
      ++next_in;
    }
    CHECK(next_in - next_out == 4);
    int item = -1;
    for (int i = 0; i < round % 4 + 1; ++i) {
      REQUIRE(ring.try_pop(item));
      REQUIRE(item == next_out++);
    }
  }
  int item = -1;
  while (ring.try_pop(item)) {
    REQUIRE(item == next_out++);
  }
  CHECK(next_out == next_in);
  CHECK_FALSE(ring.try_pop(item));
}

TEST_CASE("feed messages can travel through the ring") {
  STATIC_REQUIRE(std::is_trivially_copyable_v<pulse::wire::Message>);
  SpscRing<pulse::wire::Message> ring(2);
  const pulse::wire::Message sent{7, 9, pulse::wire::OrderDeleted{1, 2}};
  REQUIRE(ring.try_push(sent));
  pulse::wire::Message received{};
  REQUIRE(ring.try_pop(received));
  CHECK(received == sent);
}

TEST_CASE("one producer and one consumer hand over every item exactly once") {
  constexpr std::uint64_t items = 2'000'000;
  SpscRing<std::uint64_t> ring(1024);
  std::uint64_t sum = 0;
  std::uint64_t out_of_order = 0;

  std::thread consumer([&] {
    std::uint64_t expected = 1;
    std::uint64_t item = 0;
    while (expected <= items) {
      if (!ring.try_pop(item)) {
        std::this_thread::yield();
        continue;
      }
      out_of_order += item != expected ? 1 : 0;
      sum += item;
      ++expected;
    }
  });
  for (std::uint64_t item = 1; item <= items; ++item) {
    while (!ring.try_push(item)) {
      std::this_thread::yield();
    }
  }
  consumer.join();

  CHECK(out_of_order == 0);
  CHECK(sum == items * (items + 1) / 2);
}
