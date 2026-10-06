#include <array>
#include <chrono>
#include <cstdio>
#include <exception>
#include <format>
#include <stdexcept>

#include "cli.hpp"
#include "pulse/feed/feed_handler.hpp"
#include "pulse/io/capture.hpp"
#include "pulse/wire/stream_decoder.hpp"

using namespace pulse;

namespace {

constexpr const char* usage = "usage: pulse-replay FILE\n";

std::string_view describe(feed::FeedError error) {
  constexpr std::array<std::string_view, 7> names = {
      "ok",
      "sequence gap",
      "unknown instrument",
      "book rejected an event",
      "book digest mismatch",
      "wrong message count at end of session",
      "message after end of session",
  };
  return names[static_cast<std::size_t>(error)];
}

void report(const std::string& path, std::size_t bytes, const feed::FeedStats& stats,
            double seconds) {
  const double messages = static_cast<double>(stats.messages);
  std::fputs(
      std::format("replayed {}\n"
                  "  bytes             {}\n"
                  "  messages          {}\n"
                  "  adds              {}\n"
                  "  executions        {}\n"
                  "  cancels           {}\n"
                  "  deletes           {}\n"
                  "  replaces          {}\n"
                  "  digests verified  {}\n"
                  "  elapsed           {:.3f} s\n"
                  "  throughput        {:.2f} M msg/s, {:.1f} MB/s\n",
                  path, apps::with_commas(bytes), apps::with_commas(stats.messages),
                  apps::with_commas(stats.adds), apps::with_commas(stats.executions),
                  apps::with_commas(stats.cancels), apps::with_commas(stats.deletes),
                  apps::with_commas(stats.replaces), apps::with_commas(stats.digests_verified),
                  seconds, messages / seconds / 1e6, static_cast<double>(bytes) / seconds / 1e6)
          .c_str(),
      stdout);
}

int run(const apps::Args& args) {
  if (args.positional().size() != 1) {
    throw std::invalid_argument("expected one capture file");
  }
  const std::string& path = args.positional().front();
  const std::vector<std::byte> capture = io::read_capture(path);

  feed::FeedHandler handler;
  wire::StreamDecoder decoder;
  feed::FeedError error = feed::FeedError::none;
  Sequence failed_at = 0;
  const auto start = std::chrono::steady_clock::now();
  const wire::DecodeStatus status = decoder.feed(capture, [&](const wire::Message& message) {
    if (error == feed::FeedError::none) {
      error = handler.on_message(message);
      failed_at = message.sequence;
    }
  });
  const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

  if (status != wire::DecodeStatus::ok) {
    throw std::runtime_error(std::format("malformed frame at byte {}", decoder.consumed_bytes()));
  }
  if (error != feed::FeedError::none) {
    throw std::runtime_error(std::format("{} at sequence {}", describe(error), failed_at));
  }
  if (!handler.finished() || decoder.buffered_bytes() != 0) {
    throw std::runtime_error("capture ends before the end of the session");
  }
  report(path, capture.size(), handler.stats(), elapsed.count());
  std::fputs("  result            every book matched the engine's digests\n", stdout);
  return 0;
}

}

int main(int argc, char** argv) {
  try {
    return run(apps::Args(argc, argv, {}));
  } catch (const std::exception& error) {
    std::fputs(std::format("pulse-replay: {}\n{}", error.what(), usage).c_str(), stderr);
    return 1;
  }
}
