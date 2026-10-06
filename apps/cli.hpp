#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pulse::apps {

class Args {
 public:
  Args(int argc, char** argv, std::initializer_list<std::string_view> options) {
    const std::vector<std::string_view> raw(argv + 1, argv + argc);
    for (std::size_t i = 0; i < raw.size(); ++i) {
      if (!raw[i].starts_with("--")) {
        positional_.emplace_back(raw[i]);
        continue;
      }
      const std::string_view key = raw[i].substr(2);
      if (std::find(options.begin(), options.end(), key) == options.end()) {
        throw std::invalid_argument("unknown option --" + std::string(key));
      }
      if (i + 1 == raw.size()) {
        throw std::invalid_argument("missing value for --" + std::string(key));
      }
      values_[std::string(key)] = std::string(raw[++i]);
    }
  }

  [[nodiscard]] std::uint64_t number(const std::string& key, std::uint64_t fallback) const {
    const auto it = values_.find(key);
    if (it == values_.end()) {
      return fallback;
    }
    std::uint64_t value = 0;
    const std::string& text = it->second;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
      throw std::invalid_argument("--" + key + " needs a whole number, got " + text);
    }
    return value;
  }

  [[nodiscard]] std::string text(const std::string& key, const std::string& fallback) const {
    const auto it = values_.find(key);
    return it == values_.end() ? fallback : it->second;
  }

  [[nodiscard]] const std::vector<std::string>& positional() const { return positional_; }

 private:
  std::map<std::string, std::string> values_;
  std::vector<std::string> positional_;
};

inline std::string with_commas(std::uint64_t value) {
  std::string digits = std::to_string(value);
  for (std::size_t i = digits.size(); i > 3; i -= 3) {
    digits.insert(i - 3, ",");
  }
  return digits;
}

}
