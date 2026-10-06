#include "pulse/book/price_ladder.hpp"

#include <algorithm>

namespace pulse::book {

bool PriceLadder::ranks_below(Price lower, Price higher) const {
  return side_ == Side::buy ? lower < higher : lower > higher;
}

std::vector<LevelRef>::const_iterator PriceLadder::position(Price price) const {
  return std::ranges::lower_bound(
      refs_, price, [this](Price a, Price b) { return ranks_below(a, b); }, &LevelRef::price);
}

Handle PriceLadder::find(Price price) const {
  const auto it = position(price);
  return it != refs_.end() && it->price == price ? it->level : null_handle;
}

void PriceLadder::insert(Price price, Handle level) {
  refs_.insert(position(price), LevelRef{price, level});
}

void PriceLadder::erase(Price price) {
  if (!refs_.empty() && refs_.back().price == price) {
    refs_.pop_back();
    return;
  }
  const auto it = position(price);
  if (it != refs_.end() && it->price == price) {
    refs_.erase(it);
  }
}

std::optional<LevelRef> PriceLadder::best() const {
  if (refs_.empty()) {
    return std::nullopt;
  }
  return refs_.back();
}

}
