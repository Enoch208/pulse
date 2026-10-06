#pragma once

#include <chrono>

#include "pulse/types.hpp"

namespace pulse::stats {

inline Nanos now_nanos() {
  return static_cast<Nanos>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count());
}

}
