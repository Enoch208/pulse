#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace pulse::net {

class Socket {
 public:
  Socket() = default;
  explicit Socket(int fd) : fd_(fd) {}
  ~Socket();
  Socket(Socket&& other) noexcept;
  Socket& operator=(Socket&& other) noexcept;
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  [[nodiscard]] std::size_t read_some(std::span<std::byte> buffer) const;
  [[nodiscard]] std::optional<std::size_t> try_read(std::span<std::byte> buffer) const;
  void set_receive_timeout(std::chrono::milliseconds timeout) const;
  void write_all(std::span<const std::byte> bytes) const;
  void finish_writing() const;

  [[nodiscard]] int fd() const { return fd_; }

 private:
  int fd_ = -1;
};

[[nodiscard]] Socket listen_on(const std::string& host, std::uint16_t port);
[[nodiscard]] Socket accept_from(const Socket& listener);
[[nodiscard]] Socket connect_to(const std::string& host, std::uint16_t port);
[[nodiscard]] std::uint16_t local_port(const Socket& socket);

}
