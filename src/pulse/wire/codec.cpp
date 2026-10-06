#include "pulse/wire/codec.hpp"

#include <optional>
#include <type_traits>
#include <variant>

#include "pulse/wire/byte_io.hpp"
#include "pulse/wire/layout.hpp"

namespace pulse::wire {
namespace {

void write_fields(ByteWriter& writer, const AddOrder& message) {
  writer.put_u16(message.instrument);
  writer.put_u64(message.order_id);
  writer.put_u8(side_code(message.side));
  writer.put_i64(message.price);
  writer.put_u32(message.quantity);
}

void write_fields(ByteWriter& writer, const OrderExecuted& message) {
  writer.put_u16(message.instrument);
  writer.put_u64(message.order_id);
  writer.put_u32(message.quantity);
  writer.put_u64(message.match_id);
}

void write_fields(ByteWriter& writer, const OrderCancelled& message) {
  writer.put_u16(message.instrument);
  writer.put_u64(message.order_id);
  writer.put_u32(message.quantity);
}

void write_fields(ByteWriter& writer, const OrderDeleted& message) {
  writer.put_u16(message.instrument);
  writer.put_u64(message.order_id);
}

void write_fields(ByteWriter& writer, const OrderReplaced& message) {
  writer.put_u16(message.instrument);
  writer.put_u64(message.order_id);
  writer.put_u64(message.new_order_id);
  writer.put_i64(message.price);
  writer.put_u32(message.quantity);
}

void write_fields(ByteWriter& writer, const BookDigest& message) {
  writer.put_u16(message.instrument);
  writer.put_u64(message.digest);
}

void write_fields(ByteWriter& writer, const EndOfSession& message) {
  writer.put_u64(message.message_count);
}

std::optional<AddOrder> read_fields(ByteReader& reader, std::type_identity<AddOrder>) {
  const InstrumentId instrument = reader.read_u16();
  const OrderId order_id = reader.read_u64();
  const std::optional<Side> side = side_from_code(reader.read_u8());
  if (!side) {
    return std::nullopt;
  }
  const Price price = reader.read_i64();
  return AddOrder{instrument, order_id, *side, price, reader.read_u32()};
}

std::optional<OrderExecuted> read_fields(ByteReader& reader, std::type_identity<OrderExecuted>) {
  const InstrumentId instrument = reader.read_u16();
  const OrderId order_id = reader.read_u64();
  const Quantity quantity = reader.read_u32();
  return OrderExecuted{instrument, order_id, quantity, reader.read_u64()};
}

std::optional<OrderCancelled> read_fields(ByteReader& reader, std::type_identity<OrderCancelled>) {
  const InstrumentId instrument = reader.read_u16();
  const OrderId order_id = reader.read_u64();
  return OrderCancelled{instrument, order_id, reader.read_u32()};
}

std::optional<OrderDeleted> read_fields(ByteReader& reader, std::type_identity<OrderDeleted>) {
  const InstrumentId instrument = reader.read_u16();
  return OrderDeleted{instrument, reader.read_u64()};
}

std::optional<OrderReplaced> read_fields(ByteReader& reader, std::type_identity<OrderReplaced>) {
  const InstrumentId instrument = reader.read_u16();
  const OrderId order_id = reader.read_u64();
  const OrderId new_order_id = reader.read_u64();
  const Price price = reader.read_i64();
  return OrderReplaced{instrument, order_id, new_order_id, price, reader.read_u32()};
}

std::optional<BookDigest> read_fields(ByteReader& reader, std::type_identity<BookDigest>) {
  const InstrumentId instrument = reader.read_u16();
  return BookDigest{instrument, reader.read_u64()};
}

std::optional<EndOfSession> read_fields(ByteReader& reader, std::type_identity<EndOfSession>) {
  return EndOfSession{reader.read_u64()};
}

template <typename T>
DecodeResult decode_as(std::span<const std::byte> in, std::size_t length, Message& out) {
  if (length != header_size - length_prefix_size + Layout<T>::body_size) {
    return {DecodeStatus::bad_length, 0};
  }
  const std::size_t total = length_prefix_size + length;
  if (in.size() < total) {
    return {DecodeStatus::need_more, 0};
  }
  ByteReader reader(in.subspan(length_prefix_size + 1));
  const Sequence sequence = reader.read_u64();
  const Nanos timestamp = reader.read_u64();
  const std::optional<T> body = read_fields(reader, std::type_identity<T>{});
  if (!body) {
    return {DecodeStatus::bad_side, 0};
  }
  out = Message{sequence, timestamp, *body};
  return {DecodeStatus::ok, total};
}

}

std::size_t frame_size(const Body& body) {
  return std::visit(
      [](const auto& message) {
        return header_size + Layout<std::decay_t<decltype(message)>>::body_size;
      },
      body);
}

std::size_t encode(const Message& message, std::span<std::byte> out) {
  const std::size_t size = frame_size(message.body);
  if (out.size() < size) {
    return 0;
  }
  ByteWriter writer(out);
  writer.put_u16(static_cast<std::uint16_t>(size - length_prefix_size));
  std::visit(
      [&](const auto& body) {
        writer.put_u8(Layout<std::decay_t<decltype(body)>>::code);
        writer.put_u64(message.sequence);
        writer.put_u64(message.timestamp);
        write_fields(writer, body);
      },
      message.body);
  return size;
}

DecodeResult decode(std::span<const std::byte> in, Message& out) {
  if (in.size() < length_prefix_size + 1) {
    return {DecodeStatus::need_more, 0};
  }
  ByteReader reader(in);
  const std::size_t length = reader.read_u16();
  switch (reader.read_u8()) {
    case Layout<AddOrder>::code:
      return decode_as<AddOrder>(in, length, out);
    case Layout<OrderExecuted>::code:
      return decode_as<OrderExecuted>(in, length, out);
    case Layout<OrderCancelled>::code:
      return decode_as<OrderCancelled>(in, length, out);
    case Layout<OrderDeleted>::code:
      return decode_as<OrderDeleted>(in, length, out);
    case Layout<OrderReplaced>::code:
      return decode_as<OrderReplaced>(in, length, out);
    case Layout<BookDigest>::code:
      return decode_as<BookDigest>(in, length, out);
    case Layout<EndOfSession>::code:
      return decode_as<EndOfSession>(in, length, out);
    default:
      return {DecodeStatus::unknown_type, 0};
  }
}

}
