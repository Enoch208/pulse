#include <chrono>
#include <cstdio>
#include <exception>
#include <format>
#include <stdexcept>

#include "cli.hpp"
#include "pulse/feed/feed_handler.hpp"
#include "pulse/feed/session.hpp"
#include "pulse/io/capture.hpp"

using namespace pulse;

namespace {

constexpr const char* usage =
    "usage: pulse-sim [--messages N] [--instruments N] [--seed N] [--digest-every N] "
    "[--out FILE]\n";

int run(const apps::Args& args) {
  const std::uint64_t messages = args.number("messages", 1'000'000);
  const std::uint64_t instruments = args.number("instruments", 8);
  const std::uint64_t digest_every = args.number("digest-every", 10'000);
  const std::string out = args.text("out", "session.feed");
  if (instruments == 0 || instruments > feed::FeedHandler::max_instruments) {
    throw std::invalid_argument(
        std::format("--instruments must be between 1 and {}", feed::FeedHandler::max_instruments));
  }
  if (digest_every == 0) {
    throw std::invalid_argument("--digest-every must be at least 1");
  }

  sim::OrderFlow flow({args.number("seed", 1), static_cast<InstrumentId>(instruments)});
  io::CaptureWriter writer(out);
  const auto start = std::chrono::steady_clock::now();
  const Sequence published = feed::publish_session(
      flow, {messages, digest_every}, [&](const wire::Message& message) { writer.write(message); });
  writer.close();
  const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

  std::fputs(std::format("wrote {}\n"
                         "  messages     {}\n"
                         "  bytes        {}\n"
                         "  instruments  {}\n"
                         "  elapsed      {:.3f} s\n",
                         out, apps::with_commas(published),
                         apps::with_commas(writer.bytes_written()), instruments, elapsed.count())
                 .c_str(),
             stdout);
  return 0;
}

}

int main(int argc, char** argv) {
  try {
    return run(apps::Args(argc, argv, {"messages", "instruments", "seed", "digest-every", "out"}));
  } catch (const std::exception& error) {
    std::fputs(std::format("pulse-sim: {}\n{}", error.what(), usage).c_str(), stderr);
    return 2;
  }
}
