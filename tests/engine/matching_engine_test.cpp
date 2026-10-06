#include "pulse/engine/matching_engine.hpp"

#include <catch2/catch_test_macros.hpp>
#include <random>
#include <vector>

#include "../support/reference_book.hpp"
#include "../support/reference_engine.hpp"

using namespace pulse;
using namespace pulse::engine;
using pulse::wire::Body;

namespace {

constexpr InstrumentId xyz = 0;

struct Fixture {
  MatchingEngine engine{1, 64};
  std::vector<Body> events;

  void rest(OrderId id, Side side, Price price, Quantity quantity) {
    REQUIRE(engine.submit({xyz, id, side, price, quantity}, events) == Reject::none);
    events.clear();
  }
};

}

TEST_CASE("an order that does not cross rests and is published as an add") {
  Fixture f;
  f.rest(1, Side::sell, 101, 10);
  REQUIRE(f.engine.submit({xyz, 2, Side::buy, 100, 5}, f.events) == Reject::none);
  CHECK(f.events == std::vector<Body>{wire::AddOrder{xyz, 2, Side::buy, 100, 5}});
}

TEST_CASE("an aggressive order sweeps better prices first, then earlier orders") {
  Fixture f;
  f.rest(1, Side::sell, 102, 10);
  f.rest(2, Side::sell, 101, 4);
  f.rest(3, Side::sell, 101, 6);
  f.rest(4, Side::sell, 103, 10);

  REQUIRE(f.engine.submit({xyz, 9, Side::buy, 102, 25}, f.events) == Reject::none);
  CHECK(f.events == std::vector<Body>{
                        wire::OrderExecuted{xyz, 2, 4, 1},
                        wire::OrderExecuted{xyz, 3, 6, 2},
                        wire::OrderExecuted{xyz, 1, 10, 3},
                        wire::AddOrder{xyz, 9, Side::buy, 102, 5},
                    });
  CHECK(f.engine.book(xyz).best(Side::sell) == book::LevelView{103, 10, 1});
  CHECK(f.engine.book(xyz).best(Side::buy) == book::LevelView{102, 5, 1});
}

TEST_CASE("a partial fill leaves the resting order at the front of its queue") {
  Fixture f;
  f.rest(1, Side::buy, 100, 10);
  f.rest(2, Side::buy, 100, 10);

  REQUIRE(f.engine.submit({xyz, 3, Side::sell, 99, 4}, f.events) == Reject::none);
  CHECK(f.events == std::vector<Body>{wire::OrderExecuted{xyz, 1, 4, 1}});
  CHECK(f.engine.book(xyz).front(Side::buy) == book::OrderView{1, Side::buy, 100, 6});
}

TEST_CASE("cancels publish a partial cancel or a delete") {
  Fixture f;
  f.rest(1, Side::buy, 100, 10);

  REQUIRE(f.engine.cancel({xyz, 1, 3}, f.events) == Reject::none);
  REQUIRE(f.engine.cancel({xyz, 1, 50}, f.events) == Reject::none);
  CHECK(f.events == std::vector<Body>{wire::OrderCancelled{xyz, 1, 3}, wire::OrderDeleted{xyz, 1}});
  CHECK(f.engine.book(xyz).order_count() == 0);
}

TEST_CASE("a replace that would cross is a delete followed by a new aggressive order") {
  Fixture f;
  f.rest(1, Side::sell, 101, 5);
  f.rest(2, Side::buy, 99, 8);

  REQUIRE(f.engine.replace({xyz, 2, 3, 99, 8}, f.events) == Reject::none);
  REQUIRE(f.engine.replace({xyz, 3, 4, 101, 8}, f.events) == Reject::none);
  CHECK(f.events == std::vector<Body>{
                        wire::OrderReplaced{xyz, 2, 3, 99, 8},
                        wire::OrderDeleted{xyz, 3},
                        wire::OrderExecuted{xyz, 1, 5, 1},
                        wire::AddOrder{xyz, 4, Side::buy, 101, 3},
                    });
}

TEST_CASE("invalid requests are rejected without publishing anything") {
  Fixture f;
  f.rest(1, Side::buy, 100, 10);
  f.rest(2, Side::sell, 105, 10);

  CHECK(f.engine.submit({7, 3, Side::buy, 100, 1}, f.events) == Reject::unknown_instrument);
  CHECK(f.engine.submit({xyz, 0, Side::buy, 100, 1}, f.events) == Reject::invalid_order);
  CHECK(f.engine.submit({xyz, 3, Side::buy, 100, 0}, f.events) == Reject::invalid_order);
  CHECK(f.engine.submit({xyz, 1, Side::sell, 90, 1}, f.events) == Reject::duplicate_order);
  CHECK(f.engine.cancel({xyz, 9, 1}, f.events) == Reject::unknown_order);
  CHECK(f.engine.cancel({xyz, 1, 0}, f.events) == Reject::invalid_order);
  CHECK(f.engine.replace({xyz, 1, 2, 100, 5}, f.events) == Reject::duplicate_order);
  CHECK(f.engine.replace({xyz, 9, 10, 100, 5}, f.events) == Reject::unknown_order);
  CHECK(f.events.empty());
}

TEST_CASE("random request streams match a naive price-time reference exactly") {
  constexpr std::size_t instruments = 3;
  std::mt19937_64 rng(7);
  MatchingEngine engine(instruments, 64);
  test::ReferenceEngine reference(instruments);
  std::vector<Body> events;
  std::vector<Body> expected;
  OrderId next_id = 1;

  for (int step = 0; step < 100'000; ++step) {
    const auto instrument = static_cast<InstrumentId>(rng() % (instruments + 1));
    const OrderId target = rng() % (next_id + 1);
    const auto price = static_cast<Price>(1000 + rng() % 12);
    const auto quantity = static_cast<Quantity>(rng() % 20);
    const Side side = rng() % 2 == 0 ? Side::buy : Side::sell;
    events.clear();
    expected.clear();
    switch (rng() % 4) {
      case 0:
      case 1: {
        const NewOrder order{instrument, next_id++, side, price, quantity};
        REQUIRE(engine.submit(order, events) == reference.submit(order, expected));
        break;
      }
      case 2: {
        const CancelOrder request{instrument, target, quantity};
        REQUIRE(engine.cancel(request, events) == reference.cancel(request, expected));
        break;
      }
      default: {
        const ReplaceOrder request{instrument, target, next_id++, price, quantity};
        REQUIRE(engine.replace(request, events) == reference.replace(request, expected));
        break;
      }
    }
    REQUIRE(events == expected);
    if (step % 1000 == 0) {
      for (InstrumentId i = 0; i < instruments; ++i) {
        REQUIRE(test::snapshot(engine.book(i)) == reference.snapshot(i));
        REQUIRE_FALSE(engine.book(i).audit().has_value());
      }
    }
  }
}
