#include "pulse/wire/byte_io.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>

using pulse::wire::ByteReader;
using pulse::wire::ByteWriter;

TEST_CASE("integers are written least significant byte first") {
  std::array<std::byte, 4> buffer{};
  ByteWriter writer(buffer);
  writer.put_u32(0x01020304U);

  CHECK(writer.written() == 4);
  CHECK(buffer[0] == std::byte{0x04});
  CHECK(buffer[1] == std::byte{0x03});
  CHECK(buffer[2] == std::byte{0x02});
  CHECK(buffer[3] == std::byte{0x01});
}

TEST_CASE("every width round trips through its extremes") {
  std::array<std::byte, 31> buffer{};
  ByteWriter writer(buffer);
  writer.put_u8(std::numeric_limits<std::uint8_t>::max());
  writer.put_u16(std::numeric_limits<std::uint16_t>::max());
  writer.put_u32(std::numeric_limits<std::uint32_t>::max());
  writer.put_u64(std::numeric_limits<std::uint64_t>::max());
  writer.put_i64(std::numeric_limits<std::int64_t>::min());
  writer.put_i64(-1);
  REQUIRE(writer.written() == 31);

  ByteReader reader(buffer);
  CHECK(reader.read_u8() == std::numeric_limits<std::uint8_t>::max());
  CHECK(reader.read_u16() == std::numeric_limits<std::uint16_t>::max());
  CHECK(reader.read_u32() == std::numeric_limits<std::uint32_t>::max());
  CHECK(reader.read_u64() == std::numeric_limits<std::uint64_t>::max());
  CHECK(reader.read_i64() == std::numeric_limits<std::int64_t>::min());
  CHECK(reader.read_i64() == -1);
  CHECK(reader.consumed() == 31);
}
