#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "pulse/wire/messages.hpp"

namespace pulse::io {

struct FileCloser {
  void operator()(std::FILE* file) const { std::fclose(file); }
};

using FileHandle = std::unique_ptr<std::FILE, FileCloser>;

class CaptureWriter {
 public:
  explicit CaptureWriter(const std::string& path);

  void write(const wire::Message& message);
  void close();

  [[nodiscard]] std::uint64_t bytes_written() const { return bytes_written_; }

 private:
  void flush();

  FileHandle file_;
  std::vector<std::byte> buffer_;
  std::size_t used_ = 0;
  std::uint64_t bytes_written_ = 0;
};

[[nodiscard]] std::vector<std::byte> read_capture(const std::string& path);

}
