#pragma once

#include <cstddef>
#include <vector>

#include "pulse/book/handle.hpp"
#include "pulse/types.hpp"

namespace pulse::book {

class OrderIndex {
 public:
  explicit OrderIndex(std::size_t expected_orders);

  bool insert(OrderId id, Handle handle);
  [[nodiscard]] Handle find(OrderId id) const;
  bool erase(OrderId id);

  [[nodiscard]] std::size_t size() const { return size_; }
  [[nodiscard]] std::size_t capacity() const { return slots_.size(); }

 private:
  struct Slot {
    OrderId id = 0;
    Handle handle = null_handle;
  };

  [[nodiscard]] std::size_t home(OrderId id) const;
  [[nodiscard]] std::size_t probe(OrderId id) const;
  void rehash(std::size_t capacity);

  std::vector<Slot> slots_;
  std::size_t mask_ = 0;
  unsigned shift_ = 0;
  std::size_t size_ = 0;
};

}
