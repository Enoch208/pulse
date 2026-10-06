#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "pulse/book/handle.hpp"
#include "pulse/types.hpp"

namespace pulse::book {

struct LevelRef {
  Price price;
  Handle level;
};

class PriceLadder {
 public:
  explicit PriceLadder(Side side) : side_(side) {}

  [[nodiscard]] Handle find(Price price) const;
  void insert(Price price, Handle level);
  void erase(Price price);

  [[nodiscard]] std::optional<LevelRef> best() const;
  [[nodiscard]] std::size_t size() const { return refs_.size(); }
  [[nodiscard]] bool ranks_below(Price lower, Price higher) const;

  template <typename Visit>
  void for_each_best_first(Visit&& visit) const {
    for (auto it = refs_.rbegin(); it != refs_.rend(); ++it) {
      visit(*it);
    }
  }

 private:
  [[nodiscard]] std::vector<LevelRef>::const_iterator position(Price price) const;

  Side side_;
  std::vector<LevelRef> refs_;
};

}
