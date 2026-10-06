#include <array>
#include <format>
#include <variant>

#include "harness.hpp"
#include "pulse/feed/feed_handler.hpp"

using namespace pulse;

namespace {

constexpr std::uint64_t order_messages = 2'000'000;
constexpr std::array<const char*, 7> labels = {"add",     "execute", "cancel", "delete",
                                               "replace", "digest",  "end"};

void apply_all(const std::vector<wire::Message>& messages) {
  feed::FeedHandler handler;
  for (const wire::Message& message : messages) {
    if (handler.on_message(message) != feed::FeedError::none) {
      throw std::runtime_error("session did not replay cleanly");
    }
  }
}

}

int main() {
  const std::vector<wire::Message> messages = bench::recorded_session(order_messages);
  const double seconds = bench::median_seconds(5, [&] { apply_all(messages); });
  bench::print(
      std::format("book apply, {} messages, 8 instruments, median of 5 runs\n"
                  "  throughput   {:.2f} M msg/s\n"
                  "  mean         {:.1f} ns/msg\n",
                  messages.size(), static_cast<double>(messages.size()) / seconds / 1e6,
                  seconds * 1e9 / static_cast<double>(messages.size())));

  std::array<stats::LatencyHistogram, labels.size()> by_type{};
  feed::FeedHandler handler;
  for (const wire::Message& message : messages) {
    const Nanos start = stats::now_nanos();
    const feed::FeedError error = handler.on_message(message);
    by_type[message.body.index()].record(stats::now_nanos() - start);
    if (error != feed::FeedError::none) {
      throw std::runtime_error("session did not replay cleanly");
    }
  }
  bench::print(std::format("per message, timed one at a time (clock read pair costs {} ns)\n",
                           bench::clock_overhead()));
  for (std::size_t type = 0; type + 1 < labels.size(); ++type) {
    bench::print(bench::row(labels[type], by_type[type]));
  }
}
