#include "pulse/stats/latency_histogram.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace pulse::stats {

std::size_t LatencyHistogram::bucket_of(std::uint64_t nanos) {
  const std::uint64_t value = std::min(nanos, max_trackable);
  if (value < sub_buckets) {
    return static_cast<std::size_t>(value);
  }
  const auto magnitude = static_cast<unsigned>(std::bit_width(value) - 1);
  const unsigned shift = magnitude - sub_bucket_bits;
  const auto offset = static_cast<std::size_t>((value >> shift) - sub_buckets);
  return sub_buckets * (shift + 1) + offset;
}

std::uint64_t LatencyHistogram::highest_in_bucket(std::size_t bucket) {
  if (bucket < sub_buckets) {
    return bucket;
  }
  const auto shift = static_cast<unsigned>(bucket / sub_buckets - 1);
  const std::uint64_t lowest = (sub_buckets + bucket % sub_buckets) << shift;
  return lowest + (std::uint64_t{1} << shift) - 1;
}

void LatencyHistogram::record(std::uint64_t nanos) {
  ++counts_[bucket_of(nanos)];
  ++count_;
  min_ = std::min(min_, nanos);
  max_ = std::max(max_, nanos);
  total_ += static_cast<long double>(nanos);
}

void LatencyHistogram::merge(const LatencyHistogram& other) {
  for (std::size_t i = 0; i < bucket_count; ++i) {
    counts_[i] += other.counts_[i];
  }
  count_ += other.count_;
  min_ = std::min(min_, other.min_);
  max_ = std::max(max_, other.max_);
  total_ += other.total_;
}

double LatencyHistogram::mean() const {
  return count_ == 0 ? 0.0 : static_cast<double>(total_ / static_cast<long double>(count_));
}

std::uint64_t LatencyHistogram::percentile(double percent) const {
  if (count_ == 0) {
    return 0;
  }
  const double clamped = std::clamp(percent, 0.0, 100.0);
  const auto rank = std::max<std::uint64_t>(
      1, static_cast<std::uint64_t>(std::ceil(clamped / 100.0 * static_cast<double>(count_))));
  std::uint64_t seen = 0;
  for (std::size_t bucket = 0; bucket < bucket_count; ++bucket) {
    seen += counts_[bucket];
    if (seen >= rank) {
      return std::min(highest_in_bucket(bucket), max_);
    }
  }
  return max_;
}

}
