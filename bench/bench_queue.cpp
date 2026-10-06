#include <condition_variable>
#include <deque>
#include <format>
#include <mutex>
#include <thread>

#include "harness.hpp"
#include "pulse/pipeline/spsc_ring.hpp"

using namespace pulse;

namespace {

constexpr std::uint64_t items = 20'000'000;
constexpr int round_trips = 200'000;

class MutexQueue {
 public:
  bool try_push(std::uint64_t item) {
    const std::scoped_lock lock(mutex_);
    items_.push_back(item);
    return true;
  }

  bool try_pop(std::uint64_t& item) {
    const std::scoped_lock lock(mutex_);
    if (items_.empty()) {
      return false;
    }
    item = items_.front();
    items_.pop_front();
    return true;
  }

 private:
  std::mutex mutex_;
  std::deque<std::uint64_t> items_;
};

template <typename Queue>
double transfer(Queue& queue) {
  std::uint64_t sum = 0;
  const Nanos start = stats::now_nanos();
  std::thread consumer([&] {
    std::uint64_t item = 0;
    for (std::uint64_t received = 0; received < items;) {
      if (queue.try_pop(item)) {
        sum += item;
        ++received;
      }
    }
  });
  for (std::uint64_t item = 1; item <= items; ++item) {
    while (!queue.try_push(item)) {
    }
  }
  consumer.join();
  if (sum != items * (items + 1) / 2) {
    throw std::runtime_error("items lost in transfer");
  }
  return static_cast<double>(stats::now_nanos() - start) / static_cast<double>(items);
}

template <typename Queue>
stats::LatencyHistogram ping_pong(Queue& there, Queue& back) {
  stats::LatencyHistogram round_trip;
  std::thread echo([&] {
    std::uint64_t item = 0;
    for (int i = 0; i < round_trips; ++i) {
      while (!there.try_pop(item)) {
      }
      while (!back.try_push(item)) {
      }
    }
  });
  std::uint64_t item = 0;
  for (int i = 0; i < round_trips; ++i) {
    const Nanos start = stats::now_nanos();
    while (!there.try_push(start)) {
    }
    while (!back.try_pop(item)) {
    }
    round_trip.record(stats::now_nanos() - start);
  }
  echo.join();
  return round_trip;
}

}

int main() {
  pipeline::SpscRing<std::uint64_t> ring(65'536);
  MutexQueue locked;
  const double ring_ns = transfer(ring);
  const double locked_ns = transfer(locked);
  bench::print(
      std::format("two-thread transfer of {} items\n"
                  "  SpscRing                  {:.1f} ns/item\n"
                  "  std::mutex + std::deque   {:.1f} ns/item\n",
                  items, ring_ns, locked_ns));

  pipeline::SpscRing<std::uint64_t> there(64);
  pipeline::SpscRing<std::uint64_t> back(64);
  MutexQueue locked_there;
  MutexQueue locked_back;
  bench::print(std::format("round trip between two threads, {} pings\n", round_trips));
  bench::print(bench::row("SpscRing", ping_pong(there, back)));
  bench::print(bench::row("mutex", ping_pong(locked_there, locked_back)));
}
