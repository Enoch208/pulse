#pragma once

#include <cstdint>
#include <vector>

#include "pulse/book/digest.hpp"
#include "pulse/sim/order_flow.hpp"
#include "pulse/wire/messages.hpp"

namespace pulse::feed {

struct SessionPlan {
  std::uint64_t order_messages;
  std::uint64_t digest_interval;
};

inline constexpr Nanos simulated_step_nanos = 1'000;

template <typename Sink>
Sequence publish_session(sim::OrderFlow& flow, const SessionPlan& plan, Sink&& sink) {
  Sequence sequence = 0;
  Nanos clock = 0;
  const auto publish = [&](const wire::Body& body) {
    sink(wire::Message{++sequence, clock, body});
  };
  const auto publish_digests = [&] {
    for (InstrumentId i = 0; i < flow.engine().instruments(); ++i) {
      publish(wire::BookDigest{i, book::digest(flow.engine().book(i))});
    }
  };

  std::vector<wire::Body> events;
  std::uint64_t order_messages = 0;
  std::uint64_t since_digest = 0;
  while (order_messages < plan.order_messages) {
    events.clear();
    flow.step(events);
    clock += simulated_step_nanos;
    for (const wire::Body& body : events) {
      publish(body);
    }
    order_messages += events.size();
    since_digest += events.size();
    if (since_digest >= plan.digest_interval) {
      publish_digests();
      since_digest = 0;
    }
  }
  publish_digests();
  publish(wire::EndOfSession{sequence + 1});
  return sequence;
}

}
