#pragma once

#include <cstddef>
#include <vector>

#include "pulse/book/handle.hpp"

namespace pulse::book {

template <typename T>
class Slab {
 public:
  explicit Slab(std::size_t capacity) {
    items_.reserve(capacity);
    free_.reserve(capacity);
  }

  Handle acquire(const T& value) {
    if (free_.empty()) {
      items_.push_back(value);
      return static_cast<Handle>(items_.size() - 1);
    }
    const Handle handle = free_.back();
    free_.pop_back();
    items_[handle] = value;
    return handle;
  }

  void release(Handle handle) { free_.push_back(handle); }

  T& operator[](Handle handle) { return items_[handle]; }
  const T& operator[](Handle handle) const { return items_[handle]; }

  [[nodiscard]] std::size_t live() const { return items_.size() - free_.size(); }

 private:
  std::vector<T> items_;
  std::vector<Handle> free_;
};

}
