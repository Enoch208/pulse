#pragma once

#include <cstdint>
#include <limits>

namespace pulse::book {

using Handle = std::uint32_t;

inline constexpr Handle null_handle = std::numeric_limits<Handle>::max();

}
