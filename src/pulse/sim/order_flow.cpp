#include "pulse/sim/order_flow.hpp"

#include <algorithm>
#include <limits>

namespace pulse::sim {
namespace {

constexpr std::size_t expected_orders_per_book = 4096;
constexpr std::size_t shallow_book = 400;
constexpr std::size_t deep_book = 1600;
constexpr Price first_mid = 10'000;
constexpr Price mid_spacing = 2'500;
constexpr std::uint64_t mid_drift_one_in = 64;
constexpr std::uint64_t aggressive_percent = 8;
constexpr std::uint64_t full_cancel_percent = 85;
constexpr Price widest_passive_ticks = 24;
constexpr Quantity lot = 100;

}

OrderFlow::OrderFlow(const FlowConfig& config)
    : rng_(config.seed),
      engine_(config.instruments, expected_orders_per_book),
      resting_(config.instruments) {
  for (InstrumentId i = 0; i < config.instruments; ++i) {
    mids_.push_back(first_mid + mid_spacing * i);
  }
}

void OrderFlow::step(std::vector<wire::Body>& events) {
  const auto instrument = static_cast<InstrumentId>(rng_.below(mids_.size()));
  if (rng_.below(mid_drift_one_in) == 0) {
    mids_[instrument] += rng_.percent(50) ? 1 : -1;
  }
  const std::size_t depth = engine_.book(instrument).order_count();
  const std::uint64_t roll = rng_.below(100);
  if (depth < shallow_book || (depth <= deep_book && roll < 50)) {
    submit_new(instrument, events);
  } else if (roll < 80 || depth > deep_book) {
    if (!cancel_resting(instrument, events)) {
      submit_new(instrument, events);
    }
  } else if (!replace_resting(instrument, events)) {
    submit_new(instrument, events);
  }
}

void OrderFlow::submit_new(InstrumentId instrument, std::vector<wire::Body>& events) {
  const Side side = random_side();
  const Price price = rng_.percent(aggressive_percent) ? aggressive_price(instrument, side)
                                                       : passive_price(instrument, side);
  const OrderId id = next_id_++;
  engine_.submit({instrument, id, side, price, lot_quantity()}, events);
  track_if_resting(instrument, id);
}

bool OrderFlow::cancel_resting(InstrumentId instrument, std::vector<wire::Body>& events) {
  const std::optional<book::OrderView> order = pick_resting(instrument);
  if (!order) {
    return false;
  }
  const bool partial = order->quantity > 1 && !rng_.percent(full_cancel_percent);
  const Quantity quantity = partial ? static_cast<Quantity>(1 + rng_.below(order->quantity - 1))
                                    : std::numeric_limits<Quantity>::max();
  engine_.cancel({instrument, order->id, quantity}, events);
  return true;
}

bool OrderFlow::replace_resting(InstrumentId instrument, std::vector<wire::Body>& events) {
  const std::optional<book::OrderView> order = pick_resting(instrument);
  if (!order) {
    return false;
  }
  const OrderId new_id = next_id_++;
  engine_.replace(
      {instrument, order->id, new_id, passive_price(instrument, order->side), lot_quantity()},
      events);
  track_if_resting(instrument, new_id);
  return true;
}

std::optional<book::OrderView> OrderFlow::pick_resting(InstrumentId instrument) {
  std::vector<OrderId>& ids = resting_[instrument];
  while (!ids.empty()) {
    const std::size_t slot = rng_.below(ids.size());
    const std::optional<book::OrderView> order = engine_.book(instrument).find(ids[slot]);
    if (order) {
      return order;
    }
    ids[slot] = ids.back();
    ids.pop_back();
  }
  return std::nullopt;
}

void OrderFlow::track_if_resting(InstrumentId instrument, OrderId id) {
  if (engine_.book(instrument).find(id)) {
    resting_[instrument].push_back(id);
  }
}

Price OrderFlow::passive_price(InstrumentId instrument, Side side) {
  const auto distance = static_cast<Price>(
      1 + std::min(rng_.below(widest_passive_ticks), rng_.below(widest_passive_ticks)));
  return side == Side::buy ? mids_[instrument] - distance : mids_[instrument] + distance;
}

Price OrderFlow::aggressive_price(InstrumentId instrument, Side side) {
  const auto reach = static_cast<Price>(1 + rng_.below(3));
  return side == Side::buy ? mids_[instrument] + reach : mids_[instrument] - reach;
}

Quantity OrderFlow::lot_quantity() {
  return static_cast<Quantity>(lot * (1 + rng_.below(10)));
}

Side OrderFlow::random_side() {
  return rng_.percent(50) ? Side::buy : Side::sell;
}

}
