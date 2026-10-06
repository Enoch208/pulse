#pragma once

#include <cstddef>
#include <initializer_list>
#include <vector>

#include "pulse/wire/messages.hpp"

namespace pulse::test {

inline std::vector<wire::Message> sample_messages() {
  return {
      {1, 100, wire::AddOrder{7, 42, Side::buy, 10'050, 300}},
      {2, 200, wire::AddOrder{7, 43, Side::sell, -1, 1}},
      {3, 300, wire::OrderExecuted{7, 42, 100, 9'001}},
      {4, 400, wire::OrderCancelled{7, 42, 50}},
      {5, 500, wire::OrderDeleted{7, 43}},
      {6, 600, wire::OrderReplaced{7, 42, 44, 10'049, 150}},
      {7, 700, wire::BookDigest{7, 0xDEADBEEFCAFEF00DULL}},
      {8, 800, wire::EndOfSession{8}},
  };
}

inline std::vector<std::byte> bytes(std::initializer_list<int> values) {
  std::vector<std::byte> out;
  out.reserve(values.size());
  for (const int value : values) {
    out.push_back(static_cast<std::byte>(value));
  }
  return out;
}

inline std::vector<std::byte> concat(std::initializer_list<std::vector<std::byte>> parts) {
  std::vector<std::byte> out;
  for (const std::vector<std::byte>& part : parts) {
    out.insert(out.end(), part.begin(), part.end());
  }
  return out;
}

}
