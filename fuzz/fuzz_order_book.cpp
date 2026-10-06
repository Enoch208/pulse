#include <cstddef>
#include <cstdint>
#include <span>

#include "../tests/support/reference_book.hpp"
#include "pulse/book/order_book.hpp"

using namespace pulse;

namespace {

class Script {
 public:
  explicit Script(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  [[nodiscard]] bool done() const { return offset_ >= bytes_.size(); }

  std::uint8_t next() { return done() ? 0 : bytes_[offset_++]; }

 private:
  std::span<const std::uint8_t> bytes_;
  std::size_t offset_ = 0;
};

void check(bool condition) {
  if (!condition) {
    __builtin_trap();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  Script script({data, size});
  book::OrderBook book(16);
  test::ReferenceBook reference;
  while (!script.done()) {
    const std::uint8_t operation = script.next();
    const OrderId id = script.next() % 32;
    const auto quantity = static_cast<Quantity>(script.next() % 8);
    const auto price = static_cast<Price>(script.next() % 8);
    const Side side = (operation & 0x10U) != 0 ? Side::sell : Side::buy;
    switch (operation % 4) {
      case 0:
        check(book.add(id, side, price, quantity) == reference.add(id, side, price, quantity));
        break;
      case 1:
        check(book.execute(id, quantity) == reference.reduce(id, quantity));
        break;
      case 2:
        check(book.remove(id) == reference.remove(id));
        break;
      default: {
        const OrderId new_id = script.next() % 32;
        check(book.replace(id, new_id, price, quantity) ==
              reference.replace(id, new_id, price, quantity));
        break;
      }
    }
    check(!book.audit().has_value());
  }
  check(test::snapshot(book) == reference.snapshot());
  return 0;
}
