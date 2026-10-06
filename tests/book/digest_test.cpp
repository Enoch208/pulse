#include "pulse/book/digest.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace pulse;
using namespace pulse::book;

TEST_CASE("books that rest the same orders in the same queues share a digest") {
  OrderBook direct;
  REQUIRE(direct.add(1, Side::buy, 100, 6) == BookError::none);
  REQUIRE(direct.add(2, Side::sell, 101, 5) == BookError::none);

  OrderBook eventful;
  REQUIRE(eventful.add(9, Side::buy, 99, 1) == BookError::none);
  REQUIRE(eventful.add(1, Side::buy, 100, 10) == BookError::none);
  REQUIRE(eventful.add(2, Side::sell, 101, 5) == BookError::none);
  REQUIRE(eventful.execute(1, 3) == BookError::none);
  REQUIRE(eventful.cancel(1, 1) == BookError::none);
  REQUIRE(eventful.remove(9) == BookError::none);

  CHECK(digest(direct) == digest(eventful));
}

TEST_CASE("queue order, quantity, price and side all change the digest") {
  OrderBook base;
  REQUIRE(base.add(1, Side::buy, 100, 5) == BookError::none);
  REQUIRE(base.add(2, Side::buy, 100, 5) == BookError::none);
  const std::uint64_t reference = digest(base);

  OrderBook swapped;
  REQUIRE(swapped.add(2, Side::buy, 100, 5) == BookError::none);
  REQUIRE(swapped.add(1, Side::buy, 100, 5) == BookError::none);
  CHECK(digest(swapped) != reference);

  OrderBook smaller;
  REQUIRE(smaller.add(1, Side::buy, 100, 5) == BookError::none);
  REQUIRE(smaller.add(2, Side::buy, 100, 4) == BookError::none);
  CHECK(digest(smaller) != reference);

  OrderBook repriced;
  REQUIRE(repriced.add(1, Side::buy, 100, 5) == BookError::none);
  REQUIRE(repriced.add(2, Side::buy, 99, 5) == BookError::none);
  CHECK(digest(repriced) != reference);

  OrderBook other_side;
  REQUIRE(other_side.add(1, Side::buy, 100, 5) == BookError::none);
  REQUIRE(other_side.add(2, Side::sell, 100, 5) == BookError::none);
  CHECK(digest(other_side) != reference);
}

TEST_CASE("an empty book has a stable digest") {
  CHECK(digest(OrderBook{}) == digest(OrderBook{}));

  OrderBook emptied;
  REQUIRE(emptied.add(1, Side::sell, 100, 5) == BookError::none);
  REQUIRE(emptied.remove(1) == BookError::none);
  CHECK(digest(emptied) == digest(OrderBook{}));
}
