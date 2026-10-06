#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace pulse::pipeline {

inline constexpr std::size_t cache_line = 64;

template <typename T>
class SpscRing {
  static_assert(std::is_trivially_copyable_v<T>);

 public:
  explicit SpscRing(std::size_t minimum_capacity)
      : mask_(std::bit_ceil(std::max<std::size_t>(minimum_capacity, 2)) - 1),
        slots_(std::make_unique<T[]>(mask_ + 1)) {}

  bool try_push(const T& item) {
    const std::size_t tail = producer_.tail.load(std::memory_order_relaxed);
    if (tail - producer_.cached_head > mask_) {
      producer_.cached_head = consumer_.head.load(std::memory_order_acquire);
      if (tail - producer_.cached_head > mask_) {
        return false;
      }
    }
    slots_[tail & mask_] = item;
    producer_.tail.store(tail + 1, std::memory_order_release);
    return true;
  }

  bool try_pop(T& item) {
    const std::size_t head = consumer_.head.load(std::memory_order_relaxed);
    if (head == consumer_.cached_tail) {
      consumer_.cached_tail = producer_.tail.load(std::memory_order_acquire);
      if (head == consumer_.cached_tail) {
        return false;
      }
    }
    item = slots_[head & mask_];
    consumer_.head.store(head + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] std::size_t capacity() const { return mask_ + 1; }

 private:
  struct alignas(cache_line) ProducerSide {
    std::atomic<std::size_t> tail{0};
    std::size_t cached_head = 0;
  };

  struct alignas(cache_line) ConsumerSide {
    std::atomic<std::size_t> head{0};
    std::size_t cached_tail = 0;
  };

  ProducerSide producer_;
  ConsumerSide consumer_;
  const std::size_t mask_;
  const std::unique_ptr<T[]> slots_;
};

}
