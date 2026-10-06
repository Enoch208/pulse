#include "pulse/book/order_book.hpp"

#include <catch2/catch_test_macros.hpp>
#include <random>
#include <vector>

#include "../support/reference_book.hpp"

using namespace pulse;
using namespace pulse::book;
using pulse::test::snapshot;

namespace {

std::vector<OrderId> ids_on(const OrderBook& book, Side side) {
  std::vector<OrderId> ids;
  book.for_each_order(side, [&](const OrderView& order) { ids.push_back(order.id); });
  return ids;
}

void require_consistent(const OrderBook& book) {
  const std::optional<std::string> problem = book.audit();
  INFO(problem.value_or(""));
  REQUIRE_FALSE(problem.has_value());
}

}

TEST_CASE("levels are ordered best first on each side") {
  OrderBook book;
  REQUIRE(book.add(1, Side::buy, 100, 10) == BookError::none);
  REQUIRE(book.add(2, Side::buy, 102, 20) == BookError::none);
  REQUIRE(book.add(3, Side::buy, 101, 30) == BookError::none);
  REQUIRE(book.add(4, Side::sell, 105, 5) == BookError::none);
  REQUIRE(book.add(5, Side::sell, 103, 6) == BookError::none);

  CHECK(book.best(Side::buy) == LevelView{102, 20, 1});
  CHECK(book.best(Side::sell) == LevelView{103, 6, 1});
  CHECK(ids_on(book, Side::buy) == std::vector<OrderId>{2, 3, 1});
  CHECK(ids_on(book, Side::sell) == std::vector<OrderId>{5, 4});
  require_consistent(book);
}

TEST_CASE("orders at one price queue in arrival order") {
  OrderBook book;
  REQUIRE(book.add(1, Side::sell, 100, 10) == BookError::none);
  REQUIRE(book.add(2, Side::sell, 100, 20) == BookError::none);
  REQUIRE(book.add(3, Side::sell, 100, 30) == BookError::none);

  CHECK(book.best(Side::sell) == LevelView{100, 60, 3});
  CHECK(book.front(Side::sell) == OrderView{1, Side::sell, 100, 10});
  CHECK(ids_on(book, Side::sell) == std::vector<OrderId>{1, 2, 3});
}

TEST_CASE("a partial execution or cancel keeps time priority") {
  OrderBook book;
  REQUIRE(book.add(1, Side::buy, 100, 10) == BookError::none);
  REQUIRE(book.add(2, Side::buy, 100, 10) == BookError::none);

  REQUIRE(book.execute(1, 4) == BookError::none);
  REQUIRE(book.cancel(1, 2) == BookError::none);

  CHECK(book.front(Side::buy) == OrderView{1, Side::buy, 100, 4});
  CHECK(book.best(Side::buy) == LevelView{100, 14, 2});
  require_consistent(book);
}

TEST_CASE("filling the front order hands priority to the next") {
  OrderBook book;
  REQUIRE(book.add(1, Side::buy, 100, 10) == BookError::none);
  REQUIRE(book.add(2, Side::buy, 100, 10) == BookError::none);
  REQUIRE(book.add(3, Side::buy, 99, 10) == BookError::none);

  REQUIRE(book.execute(1, 10) == BookError::none);
  CHECK(book.front(Side::buy) == OrderView{2, Side::buy, 100, 10});

  REQUIRE(book.execute(2, 10) == BookError::none);
  CHECK(book.best(Side::buy) == LevelView{99, 10, 1});
  CHECK(book.level_count(Side::buy) == 1);
  CHECK_FALSE(book.find(1).has_value());
  require_consistent(book);
}

TEST_CASE("removing an order from the middle of a queue keeps the queue intact") {
  OrderBook book;
  for (OrderId id = 1; id <= 5; ++id) {
    REQUIRE(book.add(id, Side::sell, 100, 1) == BookError::none);
  }
  REQUIRE(book.remove(3) == BookError::none);
  REQUIRE(book.remove(1) == BookError::none);
  REQUIRE(book.remove(5) == BookError::none);
  CHECK(ids_on(book, Side::sell) == std::vector<OrderId>{2, 4});
  require_consistent(book);
}

TEST_CASE("a replace moves the order to the back of its new level") {
  OrderBook book;
  REQUIRE(book.add(1, Side::buy, 100, 10) == BookError::none);
  REQUIRE(book.add(2, Side::buy, 100, 10) == BookError::none);

  REQUIRE(book.replace(1, 7, 100, 10) == BookError::none);
  CHECK(ids_on(book, Side::buy) == std::vector<OrderId>{2, 7});

  REQUIRE(book.replace(7, 8, 101, 3) == BookError::none);
  CHECK(book.best(Side::buy) == LevelView{101, 3, 1});
  CHECK(book.find(8) == OrderView{8, Side::buy, 101, 3});
  require_consistent(book);
}

TEST_CASE("rejected operations leave the book unchanged") {
  OrderBook book;
  REQUIRE(book.add(1, Side::buy, 100, 10) == BookError::none);
  REQUIRE(book.add(2, Side::sell, 101, 10) == BookError::none);
  const std::vector<OrderView> before = snapshot(book);

  CHECK(book.add(0, Side::buy, 100, 1) == BookError::invalid_order_id);
  CHECK(book.add(3, Side::buy, 100, 0) == BookError::zero_quantity);
  CHECK(book.add(1, Side::sell, 105, 1) == BookError::duplicate_order_id);
  CHECK(book.execute(9, 1) == BookError::unknown_order_id);
  CHECK(book.execute(1, 11) == BookError::exceeds_quantity);
  CHECK(book.cancel(1, 0) == BookError::zero_quantity);
  CHECK(book.remove(9) == BookError::unknown_order_id);
  CHECK(book.replace(1, 2, 100, 5) == BookError::duplicate_order_id);
  CHECK(book.replace(1, 0, 100, 5) == BookError::invalid_order_id);
  CHECK(book.replace(9, 10, 100, 5) == BookError::unknown_order_id);

  CHECK(snapshot(book) == before);
  require_consistent(book);
}

TEST_CASE("random operations agree with a naive reference book") {
  std::mt19937_64 rng(42);
  OrderBook book(64);
  test::ReferenceBook reference;
  OrderId next_id = 1;

  for (int step = 0; step < 50'000; ++step) {
    const OrderId target = rng() % (next_id + 2);
    const auto quantity = static_cast<Quantity>(rng() % 12);
    const auto price = static_cast<Price>(100 + rng() % 20);
    const Side side = rng() % 2 == 0 ? Side::buy : Side::sell;
    switch (rng() % 5) {
      case 0:
      case 1:
        REQUIRE(book.add(next_id, side, price, quantity) ==
                reference.add(next_id, side, price, quantity));
        ++next_id;
        break;
      case 2:
        REQUIRE(book.execute(target, quantity) == reference.reduce(target, quantity));
        break;
      case 3:
        REQUIRE(book.remove(target) == reference.remove(target));
        break;
      default:
        REQUIRE(book.replace(target, next_id, price, quantity) ==
                reference.replace(target, next_id, price, quantity));
        ++next_id;
        break;
    }
    if (step % 500 == 0) {
      REQUIRE(snapshot(book) == reference.snapshot());
      require_consistent(book);
    }
  }
  REQUIRE(snapshot(book) == reference.snapshot());
  require_consistent(book);
}
