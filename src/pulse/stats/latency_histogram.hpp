#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pulse::stats {

class LatencyHistogram {
 public:
  static constexpr unsigned sub_bucket_bits = 7;
  static constexpr unsigned highest_bit = 40;
  static constexpr std::uint64_t max_trackable = (std::uint64_t{1} << highest_bit) - 1;

  void record(std::uint64_t nanos);
  void merge(const LatencyHistogram& other);

  [[nodiscard]] std::uint64_t count() const { return count_; }
  [[nodiscard]] std::uint64_t min() const { return count_ == 0 ? 0 : min_; }
  [[nodiscard]] std::uint64_t max() const { return max_; }
  [[nodiscard]] double mean() const;
  [[nodiscard]] std::uint64_t percentile(double percent) const;

  [[nodiscard]] static std::size_t bucket_of(std::uint64_t nanos);
  [[nodiscard]] static std::uint64_t highest_in_bucket(std::size_t bucket);

 private:
  static constexpr std::size_t sub_buckets = std::size_t{1} << sub_bucket_bits;
  static constexpr std::size_t bucket_count = sub_buckets * (highest_bit - sub_bucket_bits + 1);

  std::array<std::uint64_t, bucket_count> counts_{};
  std::uint64_t count_ = 0;
  std::uint64_t min_ = UINT64_MAX;
  std::uint64_t max_ = 0;
  long double total_ = 0;
};

}
