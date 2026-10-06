#include "pulse/io/capture.hpp"

#include <cerrno>
#include <span>
#include <system_error>

#include "pulse/wire/codec.hpp"

namespace pulse::io {
namespace {

constexpr std::size_t write_buffer_size = std::size_t{1} << 20U;

FileHandle open_file(const std::string& path, const char* mode) {
  FileHandle file(std::fopen(path.c_str(), mode));
  if (!file) {
    throw std::system_error(errno, std::generic_category(), "cannot open " + path);
  }
  return file;
}

}

CaptureWriter::CaptureWriter(const std::string& path)
    : file_(open_file(path, "wb")), buffer_(write_buffer_size) {}

void CaptureWriter::write(const wire::Message& message) {
  if (buffer_.size() - used_ < wire::max_frame_size) {
    flush();
  }
  used_ += wire::encode(message, std::span(buffer_).subspan(used_));
}

void CaptureWriter::close() {
  flush();
  if (std::fclose(file_.release()) != 0) {
    throw std::system_error(errno, std::generic_category(), "cannot close capture");
  }
}

void CaptureWriter::flush() {
  if (std::fwrite(buffer_.data(), 1, used_, file_.get()) != used_) {
    throw std::system_error(errno, std::generic_category(), "cannot write capture");
  }
  bytes_written_ += used_;
  used_ = 0;
}

std::vector<std::byte> read_capture(const std::string& path) {
  const FileHandle file = open_file(path, "rb");
  std::vector<std::byte> bytes;
  std::vector<std::byte> chunk(write_buffer_size);
  while (const std::size_t read = std::fread(chunk.data(), 1, chunk.size(), file.get())) {
    bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(read));
  }
  if (std::ferror(file.get()) != 0) {
    throw std::system_error(errno, std::generic_category(), "cannot read " + path);
  }
  return bytes;
}

}
