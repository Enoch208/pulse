#include <condition_variable>
#include <format>
#include <mutex>
#include <thread>
#include <vector>

#include "harness.hpp"
#include "pulse/pipeline/spsc_ring.hpp"

using namespace pulse;

namespace {

constexpr std::uint64_t items = 20'000'000;
constexpr int round_trips = 200'000;

class LockedRing {
 public:
  explicit LockedRing(std::size_t capacity) : slots_(capacity) {}

  bool try_push(std::uint64_t item) {
    const std::scoped_lock lock(mutex_);
    if (size_ == slots_.size()) {
      return false;
    }
    slots_[(head_ + size_) % slots_.size()] = item;
    ++size_;
    return true;
  }

  bool try_pop(std::uint64_t& item) {
    const std::scoped_lock lock(mutex_);
    if (size_ == 0) {
      return false;
    }
    item = slots_[head_];
    head_ = (head_ + 1) % slots_.size();
    --size_;
    return true;
  }

 private:
  std::mutex mutex_;
  std::vector<std::uint64_t> slots_;
  std::size_t head_ = 0;
  std::size_t size_ = 0;
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
  constexpr std::size_t capacity = 65'536;
  pipeline::SpscRing<std::uint64_t> ring(capacity);
  LockedRing locked(capacity);
  const double ring_ns = transfer(ring);
  const double locked_ns = transfer(locked);
  bench::print(
      std::format("two-thread transfer of {} items\n"
                  "  SpscRing                  {:.1f} ns/item\n"
                  "  std::mutex ring           {:.1f} ns/item\n",
                  items, ring_ns, locked_ns));

  pipeline::SpscRing<std::uint64_t> there(64);
  pipeline::SpscRing<std::uint64_t> back(64);
  LockedRing locked_there(64);
  LockedRing locked_back(64);
  bench::print(std::format("round trip between two threads, {} pings\n", round_trips));
  bench::print(bench::row("SpscRing", ping_pong(there, back)));
  bench::print(bench::row("mutex", ping_pong(locked_there, locked_back)));
}
