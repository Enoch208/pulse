#include "pulse/net/socket.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <system_error>
#include <thread>
#include <vector>

using namespace pulse::net;

TEST_CASE("bytes written in uneven pieces arrive intact and end cleanly") {
  Socket listener = listen_on("127.0.0.1", 0);
  const std::uint16_t port = local_port(listener);
  REQUIRE(port != 0);

  std::vector<std::byte> sent(1 << 20);
  for (std::size_t i = 0; i < sent.size(); ++i) {
    sent[i] = static_cast<std::byte>(i * 31 + i / 7);
  }

  std::thread client([&] {
    const Socket stream = connect_to("127.0.0.1", port);
    std::span<const std::byte> rest(sent);
    for (std::size_t piece = 1; !rest.empty(); piece = piece * 3 % 65'521 + 1) {
      const std::size_t size = std::min(piece, rest.size());
      stream.write_all(rest.first(size));
      rest = rest.subspan(size);
    }
    stream.finish_writing();
  });

  const Socket server = accept_from(listener);
  std::vector<std::byte> received;
  std::vector<std::byte> buffer(4096);
  while (const std::size_t got = server.read_some(buffer)) {
    received.insert(received.end(), buffer.begin(),
                    buffer.begin() + static_cast<std::ptrdiff_t>(got));
  }
  client.join();
  CHECK(received == sent);
}

TEST_CASE("connecting to a port nobody listens on fails loudly") {
  std::uint16_t port = 0;
  {
    const Socket listener = listen_on("127.0.0.1", 0);
    port = local_port(listener);
  }
  CHECK_THROWS_AS(connect_to("127.0.0.1", port), std::system_error);
}

TEST_CASE("a read with a timeout returns empty-handed when nothing arrives") {
  const Socket listener = listen_on("127.0.0.1", 0);
  const Socket client = connect_to("127.0.0.1", local_port(listener));
  const Socket server = accept_from(listener);
  server.set_receive_timeout(std::chrono::milliseconds(50));

  std::vector<std::byte> buffer(16);
  const auto start = std::chrono::steady_clock::now();
  CHECK_FALSE(server.try_read(buffer).has_value());
  const auto waited = std::chrono::steady_clock::now() - start;
  CHECK(waited >= std::chrono::milliseconds(40));
  CHECK(waited < std::chrono::seconds(2));

  client.write_all(std::span(buffer).first(3));
  CHECK(server.try_read(buffer) == std::optional<std::size_t>(3));
  client.finish_writing();
  CHECK(server.try_read(buffer) == std::optional<std::size_t>(0));
}
