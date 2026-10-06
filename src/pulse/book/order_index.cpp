#include "pulse/book/order_index.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace pulse::book {
namespace {

constexpr std::uint64_t fibonacci_multiplier = 0x9E3779B97F4A7C15ULL;
constexpr std::size_t minimum_capacity = 16;

std::size_t capacity_for(std::size_t expected_orders) {
  return std::bit_ceil(std::max(minimum_capacity, expected_orders * 2));
}

}

OrderIndex::OrderIndex(std::size_t expected_orders) {
  rehash(capacity_for(expected_orders));
}

bool OrderIndex::insert(OrderId id, Handle handle) {
  if (id == 0) {
    return false;
  }
  if ((size_ + 1) * 2 > slots_.size()) {
    rehash(slots_.size() * 2);
  }
  const std::size_t slot = probe(id);
  if (slots_[slot].id == id) {
    return false;
  }
  slots_[slot] = Slot{id, handle};
  ++size_;
  return true;
}

Handle OrderIndex::find(OrderId id) const {
  const Slot& slot = slots_[probe(id)];
  return slot.id == id && id != 0 ? slot.handle : null_handle;
}

bool OrderIndex::erase(OrderId id) {
  std::size_t hole = probe(id);
  if (id == 0 || slots_[hole].id != id) {
    return false;
  }
  for (std::size_t next = (hole + 1) & mask_; slots_[next].id != 0; next = (next + 1) & mask_) {
    const std::size_t displacement = (next - home(slots_[next].id)) & mask_;
    if (displacement >= ((next - hole) & mask_)) {
      slots_[hole] = slots_[next];
      hole = next;
    }
  }
  slots_[hole] = Slot{};
  --size_;
  return true;
}

std::size_t OrderIndex::home(OrderId id) const {
  return static_cast<std::size_t>((id * fibonacci_multiplier) >> shift_);
}

std::size_t OrderIndex::probe(OrderId id) const {
  std::size_t slot = home(id);
  while (slots_[slot].id != 0 && slots_[slot].id != id) {
    slot = (slot + 1) & mask_;
  }
  return slot;
}

void OrderIndex::rehash(std::size_t capacity) {
  std::vector<Slot> previous(capacity);
  previous.swap(slots_);
  mask_ = capacity - 1;
  shift_ = static_cast<unsigned>(64 - std::countr_zero(capacity));
  size_ = 0;
  for (const Slot& slot : previous) {
    if (slot.id != 0) {
      slots_[probe(slot.id)] = slot;
      ++size_;
    }
  }
}

}
