#include "pulse/book/order_index.hpp"

#include <catch2/catch_test_macros.hpp>
#include <random>
#include <unordered_map>

using namespace pulse;
using namespace pulse::book;

namespace {

void check_matches(const OrderIndex& index, const std::unordered_map<OrderId, Handle>& expected,
                   OrderId key_limit) {
  REQUIRE(index.size() == expected.size());
  for (OrderId id = 1; id <= key_limit; ++id) {
    const auto it = expected.find(id);
    REQUIRE(index.find(id) == (it == expected.end() ? null_handle : it->second));
  }
}

}

TEST_CASE("insert, find and erase behave like a map") {
  OrderIndex index(4);
  CHECK(index.insert(10, 1));
  CHECK(index.insert(20, 2));
  CHECK_FALSE(index.insert(10, 3));
  CHECK(index.find(10) == 1);
  CHECK(index.find(20) == 2);
  CHECK(index.find(30) == null_handle);
  CHECK(index.erase(10));
  CHECK_FALSE(index.erase(10));
  CHECK(index.find(10) == null_handle);
  CHECK(index.find(20) == 2);
  CHECK(index.size() == 1);
}

TEST_CASE("order id zero is never stored") {
  OrderIndex index(4);
  CHECK_FALSE(index.insert(0, 1));
  CHECK(index.find(0) == null_handle);
  CHECK_FALSE(index.erase(0));
  CHECK(index.size() == 0);
}

TEST_CASE("the table grows and keeps every entry") {
  OrderIndex index(1);
  const std::size_t initial = index.capacity();
  for (OrderId id = 1; id <= 10'000; ++id) {
    REQUIRE(index.insert(id * 7919, static_cast<Handle>(id)));
  }
  CHECK(index.capacity() > initial);
  CHECK(index.size() * 2 <= index.capacity());
  for (OrderId id = 1; id <= 10'000; ++id) {
    REQUIRE(index.find(id * 7919) == static_cast<Handle>(id));
  }
}

TEST_CASE("random operations agree with std::unordered_map") {
  constexpr OrderId key_limit = 512;
  std::mt19937_64 rng(20261006);
  OrderIndex index(64);
  std::unordered_map<OrderId, Handle> expected;

  for (int step = 0; step < 200'000; ++step) {
    const OrderId id = rng() % key_limit + 1;
    const auto handle = static_cast<Handle>(rng() % 1'000'000);
    if (rng() % 3 == 0) {
      REQUIRE(index.erase(id) == (expected.erase(id) == 1));
    } else {
      REQUIRE(index.insert(id, handle) == expected.emplace(id, handle).second);
    }
    if (step % 10'000 == 0) {
      check_matches(index, expected, key_limit);
    }
  }
  check_matches(index, expected, key_limit);
}
