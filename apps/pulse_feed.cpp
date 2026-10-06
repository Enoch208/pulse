#include <chrono>
#include <cstdio>
#include <exception>
#include <format>
#include <stdexcept>
#include <thread>
#include <vector>

#include "cli.hpp"
#include "pulse/io/capture.hpp"
#include "pulse/net/socket.hpp"
#include "pulse/stats/clock.hpp"
#include "pulse/wire/codec.hpp"

using namespace pulse;

namespace {

constexpr const char* usage =
    "usage: pulse-feed FILE [--host ADDRESS] [--port N] [--rate MESSAGES_PER_SECOND]\n";
constexpr std::size_t batch_bytes = 64 * 1024;
constexpr Nanos sleep_threshold = 200'000;
constexpr Nanos wake_margin = 100'000;

std::size_t frame_length(std::span<const std::byte> rest) {
  if (rest.size() < wire::length_prefix_size) {
    throw std::runtime_error("capture ends inside a frame");
  }
  const std::size_t length = wire::length_prefix_size + std::to_integer<std::size_t>(rest[0]) +
                             (std::to_integer<std::size_t>(rest[1]) << 8U);
  if (length < wire::header_size || length > rest.size()) {
    throw std::runtime_error("capture holds a malformed frame");
  }
  return length;
}

void wait_until(Nanos deadline) {
  for (Nanos now = stats::now_nanos(); now < deadline; now = stats::now_nanos()) {
    if (deadline - now > sleep_threshold) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(deadline - now - wake_margin));
    } else {
      std::this_thread::yield();
    }
  }
}

struct Publication {
  std::uint64_t frames = 0;
  std::uint64_t writes = 0;
  double seconds = 0;
};

Publication publish(const net::Socket& client, std::span<const std::byte> capture,
                    std::uint64_t rate) {
  std::vector<std::byte> batch;
  batch.reserve(batch_bytes + wire::max_frame_size);
  Publication result;
  const auto flush = [&] {
    if (!batch.empty()) {
      client.write_all(batch);
      batch.clear();
      ++result.writes;
    }
  };

  const Nanos start = stats::now_nanos();
  for (std::size_t offset = 0; offset < capture.size(); ++result.frames) {
    const std::size_t length = frame_length(capture.subspan(offset));
    Nanos stamp = stats::now_nanos();
    if (rate != 0) {
      const Nanos intended = start + result.frames * 1'000'000'000ULL / rate;
      if (stamp < intended) {
        flush();
        wait_until(intended);
      }
      stamp = intended;
    }
    const std::size_t at = batch.size();
    batch.insert(batch.end(), capture.begin() + static_cast<std::ptrdiff_t>(offset),
                 capture.begin() + static_cast<std::ptrdiff_t>(offset + length));
    wire::restamp(std::span(batch).subspan(at), stamp);
    offset += length;
    if (batch.size() >= batch_bytes) {
      flush();
    }
  }
  flush();
  client.finish_writing();
  result.seconds = static_cast<double>(stats::now_nanos() - start) / 1e9;
  return result;
}

int run(const apps::Args& args) {
  if (args.positional().size() != 1) {
    throw std::invalid_argument("expected one capture file");
  }
  const std::string& path = args.positional().front();
  const std::string host = args.text("host", "127.0.0.1");
  const std::uint64_t port = args.number("port", 9100);
  const std::uint64_t rate = args.number("rate", 0);
  if (port > 65'535) {
    throw std::invalid_argument("--port must be below 65536");
  }

  const std::vector<std::byte> capture = io::read_capture(path);
  const net::Socket listener = net::listen_on(host, static_cast<std::uint16_t>(port));
  std::fputs(std::format("serving {} on {}:{}\n", path, host, net::local_port(listener)).c_str(),
             stdout);
  std::fflush(stdout);
  const net::Socket client = net::accept_from(listener);
  const Publication sent = publish(client, capture, rate);

  std::fputs(std::format("published {} frames in {} writes\n"
                         "  elapsed       {:.3f} s\n"
                         "  target rate   {}\n"
                         "  actual rate   {:.0f} msg/s\n",
                         apps::with_commas(sent.frames), apps::with_commas(sent.writes),
                         sent.seconds, rate == 0 ? "unpaced" : apps::with_commas(rate) + " msg/s",
                         static_cast<double>(sent.frames) / sent.seconds)
                 .c_str(),
             stdout);
  return 0;
}

}

int main(int argc, char** argv) {
  try {
    return run(apps::Args(argc, argv, {"host", "port", "rate"}));
  } catch (const std::exception& error) {
    std::fputs(std::format("pulse-feed: {}\n{}", error.what(), usage).c_str(), stderr);
    return 1;
  }
}
