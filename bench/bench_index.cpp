#include <format>
#include <unordered_map>

#include "harness.hpp"
#include "pulse/book/order_index.hpp"
#include "pulse/sim/rng.hpp"

using namespace pulse;

namespace {

constexpr std::size_t live_orders = 1'000'000;
constexpr std::size_t operations = 10'000'000;

struct FlatIndex {
  book::OrderIndex index{live_orders};
  bool insert(OrderId id, book::Handle handle) { return index.insert(id, handle); }
  bool contains(OrderId id) const { return index.find(id) != book::null_handle; }
  bool erase(OrderId id) { return index.erase(id); }
};

struct NodeMap {
  NodeMap() { map.reserve(live_orders); }
  std::unordered_map<OrderId, book::Handle> map;
  bool insert(OrderId id, book::Handle handle) { return map.emplace(id, handle).second; }
  bool contains(OrderId id) const { return map.contains(id); }
  bool erase(OrderId id) { return map.erase(id) == 1; }
};

template <typename Index>
double churn() {
  Index index;
  std::vector<OrderId> live;
  OrderId next = 1;
  for (; next <= live_orders; ++next) {
    index.insert(next * 3, static_cast<book::Handle>(next));
    live.push_back(next * 3);
  }
  sim::Rng rng(9);
  std::size_t found = 0;
  const Nanos start = stats::now_nanos();
  for (std::size_t i = 0; i < operations; ++i) {
    const std::size_t slot = rng.below(live.size());
    if (rng.below(10) < 6) {
      found += index.contains(live[slot]) ? 1U : 0U;
    } else {
      index.erase(live[slot]);
      live[slot] = ++next * 3;
      index.insert(live[slot], static_cast<book::Handle>(next));
    }
  }
  const auto seconds = static_cast<double>(stats::now_nanos() - start) / 1e9;
  if (found == 0) {
    throw std::runtime_error("lookups found nothing");
  }
  return seconds * 1e9 / static_cast<double>(operations);
}

}

int main() {
  const double flat = churn<FlatIndex>();
  const double node = churn<NodeMap>();
  bench::print(
      std::format("order id index, {} live ids, {} operations (60% find, 40% erase+insert)\n"
                  "  OrderIndex (open addressing)  {:.1f} ns/op\n"
                  "  std::unordered_map            {:.1f} ns/op\n",
                  live_orders, operations, flat, node));
}
