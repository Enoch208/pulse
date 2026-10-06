#include "pulse/sim/order_flow.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "pulse/book/digest.hpp"
#include "pulse/wire/codec.hpp"

using namespace pulse;
using pulse::sim::FlowConfig;
using pulse::sim::OrderFlow;

namespace {

std::vector<wire::Body> run(const FlowConfig& config, int steps) {
  OrderFlow flow(config);
  std::vector<wire::Body> events;
  for (int i = 0; i < steps; ++i) {
    flow.step(events);
  }
  return events;
}

std::uint64_t fingerprint(const std::vector<wire::Body>& events) {
  book::Fnv1a hash;
  std::array<std::byte, wire::max_frame_size> frame{};
  Sequence sequence = 0;
  for (const wire::Body& body : events) {
    const std::size_t size = wire::encode({++sequence, 0, body}, frame);
    for (std::size_t i = 0; i < size; ++i) {
      hash.add(std::to_integer<std::uint64_t>(frame[i]));
    }
  }
  return hash.value();
}

}

TEST_CASE("the same seed produces the same feed") {
  CHECK(run({11, 4}, 20'000) == run({11, 4}, 20'000));
  CHECK(run({11, 4}, 20'000) != run({12, 4}, 20'000));
}

TEST_CASE("the feed is identical on every compiler and platform") {
  const std::vector<wire::Body> events = run({1, 4}, 20'000);
  CHECK(fingerprint(events) == 0x38FB2D2096D3A7BFULL);
}

TEST_CASE("a long session exercises every order event and keeps books bounded") {
  OrderFlow flow({3, 4});
  std::vector<wire::Body> events;
  std::array<std::size_t, std::variant_size_v<wire::Body>> seen{};
  for (int i = 0; i < 200'000; ++i) {
    events.clear();
    flow.step(events);
    for (const wire::Body& body : events) {
      ++seen[body.index()];
    }
  }
  for (std::size_t type = 0; type < 5; ++type) {
    INFO("event type " << type);
    CHECK(seen[type] > 1'000);
  }
  for (InstrumentId i = 0; i < 4; ++i) {
    const book::OrderBook& book = flow.engine().book(i);
    CHECK(book.order_count() < 2'000);
    CHECK_FALSE(book.audit().has_value());
    REQUIRE(book.best(Side::buy).has_value());
    REQUIRE(book.best(Side::sell).has_value());
    CHECK(book.best(Side::buy)->price < book.best(Side::sell)->price);
  }
}
