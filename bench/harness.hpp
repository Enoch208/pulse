#pragma once

#include <algorithm>
#include <cstdio>
#include <format>
#include <string>
#include <vector>

#include "pulse/feed/session.hpp"
#include "pulse/stats/clock.hpp"
#include "pulse/stats/latency_histogram.hpp"

namespace pulse::bench {

inline std::vector<wire::Message> recorded_session(std::uint64_t order_messages) {
  sim::OrderFlow flow({1, 8});
  std::vector<wire::Message> messages;
  messages.reserve(order_messages + order_messages / 1000);
  feed::publish_session(flow, {order_messages, 10'000},
                        [&](const wire::Message& message) { messages.push_back(message); });
  return messages;
}

template <typename Run>
double median_seconds(int repetitions, Run&& run) {
  std::vector<double> seconds;
  for (int i = 0; i < repetitions; ++i) {
    const Nanos start = stats::now_nanos();
    run();
    seconds.push_back(static_cast<double>(stats::now_nanos() - start) / 1e9);
  }
  std::ranges::sort(seconds);
  return seconds[seconds.size() / 2];
}

inline Nanos clock_overhead() {
  stats::LatencyHistogram histogram;
  for (int i = 0; i < 1'000'000; ++i) {
    const Nanos start = stats::now_nanos();
    histogram.record(stats::now_nanos() - start);
  }
  return histogram.percentile(50);
}

inline void print(const std::string& text) {
  std::fputs(text.c_str(), stdout);
}

inline std::string row(const std::string& label, const stats::LatencyHistogram& histogram) {
  return std::format("  {:<12} {:>10}  p50 {:>6} ns  p90 {:>6} ns  p99 {:>6} ns  p99.9 {:>7} ns\n",
                     label, histogram.count(), histogram.percentile(50), histogram.percentile(90),
                     histogram.percentile(99), histogram.percentile(99.9));
}

}
