#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <format>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "cli.hpp"
#include "pulse/feed/feed_handler.hpp"
#include "pulse/net/socket.hpp"
#include "pulse/pipeline/spsc_ring.hpp"
#include "pulse/stats/clock.hpp"
#include "pulse/stats/latency_histogram.hpp"
#include "pulse/wire/stream_decoder.hpp"

using namespace pulse;

namespace {

constexpr const char* usage = "usage: pulse-book [--host ADDRESS] [--port N] [--ring SLOTS]\n";
constexpr std::size_t read_size = 64 * 1024;
constexpr int connect_attempts = 50;

struct Shared {
  explicit Shared(std::size_t slots) : ring(slots) {}

  pipeline::SpscRing<wire::Message> ring;
  std::atomic<bool> network_done{false};
  std::atomic<bool> stop{false};
  std::string network_error;
};

struct Consumed {
  feed::FeedError error = feed::FeedError::none;
  Sequence failed_at = 0;
  stats::LatencyHistogram latency;
  Nanos first = 0;
  Nanos last = 0;
};

net::Socket connect_with_retry(const std::string& host, std::uint16_t port) {
  for (int attempt = 1;; ++attempt) {
    try {
      return net::connect_to(host, port);
    } catch (const std::system_error&) {
      if (attempt == connect_attempts) {
        throw;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

void receive(const net::Socket& socket, Shared& shared) {
  try {
    std::vector<std::byte> buffer(read_size);
    wire::StreamDecoder decoder;
    const auto forward = [&](const wire::Message& message) {
      while (!shared.ring.try_push(message) && !shared.stop.load(std::memory_order_relaxed)) {
        std::this_thread::yield();
      }
    };
    while (!shared.stop.load(std::memory_order_relaxed)) {
      const std::size_t received = socket.read_some(buffer);
      if (received == 0) {
        break;
      }
      if (decoder.feed(std::span(buffer).first(received), forward) != wire::DecodeStatus::ok) {
        shared.network_error = std::format("malformed frame at byte {}", decoder.consumed_bytes());
        break;
      }
    }
  } catch (const std::exception& error) {
    shared.network_error = error.what();
  }
  shared.network_done.store(true, std::memory_order_release);
}

bool next_message(Shared& shared, wire::Message& message) {
  while (!shared.ring.try_pop(message)) {
    if (shared.network_done.load(std::memory_order_acquire)) {
      return shared.ring.try_pop(message);
    }
    std::this_thread::yield();
  }
  return true;
}

Consumed consume(Shared& shared, feed::FeedHandler& handler) {
  Consumed result;
  wire::Message message{};
  while (!handler.finished() && next_message(shared, message)) {
    const feed::FeedError error = handler.on_message(message);
    const Nanos applied = stats::now_nanos();
    result.latency.record(applied > message.timestamp ? applied - message.timestamp : 0);
    result.first = result.first == 0 ? applied : result.first;
    result.last = applied;
    if (error != feed::FeedError::none) {
      result.error = error;
      result.failed_at = message.sequence;
      break;
    }
  }
  shared.stop.store(true, std::memory_order_relaxed);
  return result;
}

std::string micros(std::uint64_t nanos) {
  return std::format("{:>9.1f} us", static_cast<double>(nanos) / 1e3);
}

void report(const feed::FeedHandler& handler, const Consumed& consumed) {
  const double seconds = static_cast<double>(consumed.last - consumed.first) / 1e9;
  const stats::LatencyHistogram& latency = consumed.latency;
  std::fputs(
      std::format("applied {} messages\n"
                  "  digests verified  {}\n"
                  "  elapsed           {:.3f} s\n"
                  "  throughput        {:.2f} M msg/s\n"
                  "  latency from intended send time to book applied\n"
                  "    p50    {}\n    p90    {}\n    p99    {}\n    p99.9  {}\n    max    {}\n"
                  "  result            every book matched the engine's digests\n",
                  apps::with_commas(handler.stats().messages),
                  apps::with_commas(handler.stats().digests_verified), seconds,
                  static_cast<double>(handler.stats().messages) / seconds / 1e6,
                  micros(latency.percentile(50)), micros(latency.percentile(90)),
                  micros(latency.percentile(99)), micros(latency.percentile(99.9)),
                  micros(latency.max()))
          .c_str(),
      stdout);
}

int run(const apps::Args& args) {
  const std::string host = args.text("host", "127.0.0.1");
  const std::uint64_t port = args.number("port", 9100);
  if (port == 0 || port > 65'535) {
    throw std::invalid_argument("--port must be between 1 and 65535");
  }
  Shared shared(args.number("ring", 65'536));
  const net::Socket socket = connect_with_retry(host, static_cast<std::uint16_t>(port));
  feed::FeedHandler handler;

  std::thread network([&] { receive(socket, shared); });
  const Consumed consumed = consume(shared, handler);
  network.join();

  if (consumed.error != feed::FeedError::none) {
    throw std::runtime_error(
        std::format("{} at sequence {}", feed::describe(consumed.error), consumed.failed_at));
  }
  if (!shared.network_error.empty()) {
    throw std::runtime_error(shared.network_error);
  }
  if (!handler.finished()) {
    throw std::runtime_error("connection closed before the end of the session");
  }
  report(handler, consumed);
  return 0;
}

}

int main(int argc, char** argv) {
  try {
    return run(apps::Args(argc, argv, {"host", "port", "ring"}));
  } catch (const std::exception& error) {
    std::fputs(std::format("pulse-book: {}\n{}", error.what(), usage).c_str(), stderr);
    return 1;
  }
}
