#include "pulse/net/socket.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace pulse::net {
namespace {

#ifdef MSG_NOSIGNAL
constexpr int send_flags = MSG_NOSIGNAL;
#else
constexpr int send_flags = 0;
#endif

[[noreturn]] void fail(const std::string& what) {
  throw std::system_error(errno, std::system_category(), what);
}

void set_option(int fd, int level, int option) {
  const int enabled = 1;
  if (::setsockopt(fd, level, option, &enabled, sizeof(enabled)) != 0) {
    fail("setsockopt");
  }
}

void tune_stream(int fd) {
  set_option(fd, IPPROTO_TCP, TCP_NODELAY);
#ifdef SO_NOSIGPIPE
  set_option(fd, SOL_SOCKET, SO_NOSIGPIPE);
#endif
}

using AddressList = std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)>;

AddressList resolve(const std::string& host, std::uint16_t port, int flags) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = flags | AI_NUMERICSERV;
  addrinfo* found = nullptr;
  const int status = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found);
  if (status != 0) {
    throw std::runtime_error("cannot resolve " + host + ": " + ::gai_strerror(status));
  }
  return AddressList(found, &::freeaddrinfo);
}

template <typename Setup>
Socket first_working(const AddressList& addresses, const std::string& what, Setup&& setup) {
  for (const addrinfo* address = addresses.get(); address != nullptr; address = address->ai_next) {
    Socket socket(::socket(address->ai_family, address->ai_socktype, address->ai_protocol));
    if (socket.fd() >= 0 && setup(socket.fd(), *address)) {
      return socket;
    }
  }
  fail(what);
}

}

Socket::~Socket() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

Socket::Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
  if (this != &other) {
    Socket closing(std::exchange(fd_, std::exchange(other.fd_, -1)));
  }
  return *this;
}

std::size_t Socket::read_some(std::span<std::byte> buffer) const {
  while (true) {
    const ssize_t received = ::recv(fd_, buffer.data(), buffer.size(), 0);
    if (received >= 0) {
      return static_cast<std::size_t>(received);
    }
    if (errno != EINTR) {
      fail("recv");
    }
  }
}

void Socket::write_all(std::span<const std::byte> bytes) const {
  while (!bytes.empty()) {
    const ssize_t sent = ::send(fd_, bytes.data(), bytes.size(), send_flags);
    if (sent < 0) {
      if (errno == EINTR) {
        continue;
      }
      fail("send");
    }
    bytes = bytes.subspan(static_cast<std::size_t>(sent));
  }
}

void Socket::finish_writing() const {
  if (::shutdown(fd_, SHUT_WR) != 0) {
    fail("shutdown");
  }
}

Socket listen_on(const std::string& host, std::uint16_t port) {
  return first_working(resolve(host, port, AI_PASSIVE), "cannot listen on " + host,
                       [](int fd, const addrinfo& address) {
                         set_option(fd, SOL_SOCKET, SO_REUSEADDR);
                         return ::bind(fd, address.ai_addr, address.ai_addrlen) == 0 &&
                                ::listen(fd, 1) == 0;
                       });
}

Socket accept_from(const Socket& listener) {
  while (true) {
    Socket socket(::accept(listener.fd(), nullptr, nullptr));
    if (socket.fd() >= 0) {
      tune_stream(socket.fd());
      return socket;
    }
    if (errno != EINTR) {
      fail("accept");
    }
  }
}

Socket connect_to(const std::string& host, std::uint16_t port) {
  Socket socket = first_working(resolve(host, port, 0),
                                "cannot connect to " + host + ":" + std::to_string(port),
                                [](int fd, const addrinfo& address) {
                                  return ::connect(fd, address.ai_addr, address.ai_addrlen) == 0;
                                });
  tune_stream(socket.fd());
  return socket;
}

std::uint16_t local_port(const Socket& socket) {
  sockaddr_storage storage{};
  socklen_t length = sizeof(storage);
  if (::getsockname(socket.fd(), reinterpret_cast<sockaddr*>(&storage), &length) != 0) {
    fail("getsockname");
  }
  std::array<unsigned char, 2> network_order{};
  if (storage.ss_family == AF_INET6) {
    sockaddr_in6 address{};
    std::memcpy(&address, &storage, sizeof(address));
    std::memcpy(network_order.data(), &address.sin6_port, network_order.size());
  } else {
    sockaddr_in address{};
    std::memcpy(&address, &storage, sizeof(address));
    std::memcpy(network_order.data(), &address.sin_port, network_order.size());
  }
  return static_cast<std::uint16_t>((network_order[0] << 8U) | network_order[1]);
}

}
