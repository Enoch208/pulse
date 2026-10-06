#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "pulse/engine/matching_engine.hpp"
#include "pulse/sim/rng.hpp"
#include "pulse/wire/messages.hpp"

namespace pulse::sim {

struct FlowConfig {
  std::uint64_t seed;
  InstrumentId instruments;
};

class OrderFlow {
 public:
  explicit OrderFlow(const FlowConfig& config);

  void step(std::vector<wire::Body>& events);

  [[nodiscard]] const engine::MatchingEngine& engine() const { return engine_; }

 private:
  void submit_new(InstrumentId instrument, std::vector<wire::Body>& events);
  bool cancel_resting(InstrumentId instrument, std::vector<wire::Body>& events);
  bool replace_resting(InstrumentId instrument, std::vector<wire::Body>& events);
  std::optional<book::OrderView> pick_resting(InstrumentId instrument);
  void track_if_resting(InstrumentId instrument, OrderId id);

  Price passive_price(InstrumentId instrument, Side side);
  Price aggressive_price(InstrumentId instrument, Side side);
  Quantity lot_quantity();
  Side random_side();

  Rng rng_;
  engine::MatchingEngine engine_;
  std::vector<Price> mids_;
  std::vector<std::vector<OrderId>> resting_;
  OrderId next_id_ = 1;
};

}
