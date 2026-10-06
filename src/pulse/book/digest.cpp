#include "pulse/book/digest.hpp"

namespace pulse::book {

std::uint64_t digest(const OrderBook& book) {
  Fnv1a hash;
  for (const Side side : {Side::buy, Side::sell}) {
    std::uint64_t orders = 0;
    hash.add(static_cast<std::uint64_t>(side));
    book.for_each_order(side, [&](const OrderView& order) {
      hash.add(order.id);
      hash.add(static_cast<std::uint64_t>(order.price));
      hash.add(order.quantity);
      ++orders;
    });
    hash.add(orders);
  }
  return hash.value();
}

}
