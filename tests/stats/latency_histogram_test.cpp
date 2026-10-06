#include "pulse/stats/latency_histogram.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <random>
#include <vector>

using pulse::stats::LatencyHistogram;

TEST_CASE("values below 128 ns are recorded exactly") {
  for (std::uint64_t value = 0; value < 128; ++value) {
    CHECK(LatencyHistogram::bucket_of(value) == value);
    CHECK(LatencyHistogram::highest_in_bucket(value) == value);
  }
}

TEST_CASE("every bucket reports within 1 percent of the values it holds") {
  std::mt19937_64 rng(5);
  for (int i = 0; i < 100'000; ++i) {
    const std::uint64_t value = rng() >> (rng() % 40 + 24);
    const std::uint64_t reported =
        LatencyHistogram::highest_in_bucket(LatencyHistogram::bucket_of(value));
    REQUIRE(reported >= value);
    REQUIRE(static_cast<double>(reported - value) <= static_cast<double>(value) / 128.0);
  }
}

TEST_CASE("bucket boundaries are contiguous") {
  for (std::size_t bucket = 1; bucket < 4000; ++bucket) {
    const std::uint64_t first = LatencyHistogram::highest_in_bucket(bucket - 1) + 1;
    REQUIRE(LatencyHistogram::bucket_of(first) == bucket);
    REQUIRE(LatencyHistogram::bucket_of(LatencyHistogram::highest_in_bucket(bucket)) == bucket);
  }
}

TEST_CASE("percentiles track a known distribution") {
  LatencyHistogram histogram;
  std::vector<std::uint64_t> values;
  for (std::uint64_t value = 1; value <= 100'000; ++value) {
    histogram.record(value);
    values.push_back(value);
  }
  for (const double percent : {50.0, 90.0, 99.0, 99.9}) {
    const auto exact = static_cast<double>(values[static_cast<std::size_t>(percent * 1'000) - 1]);
    const auto measured = static_cast<double>(histogram.percentile(percent));
    CHECK(measured >= exact);
    CHECK(measured <= exact * 1.01);
  }
  CHECK(histogram.percentile(100) == 100'000);
  CHECK(histogram.min() == 1);
  CHECK(histogram.max() == 100'000);
  CHECK(std::abs(histogram.mean() - 50'000.5) < 1e-6);
}

TEST_CASE("merging two histograms equals recording everything in one") {
  LatencyHistogram left;
  LatencyHistogram right;
  LatencyHistogram both;
  for (std::uint64_t value = 1; value < 50'000; value += 7) {
    (value % 2 == 0 ? left : right).record(value * 3);
    both.record(value * 3);
  }
  left.merge(right);
  CHECK(left.count() == both.count());
  CHECK(left.min() == both.min());
  CHECK(left.max() == both.max());
  for (const double percent : {1.0, 50.0, 99.0, 99.99}) {
    CHECK(left.percentile(percent) == both.percentile(percent));
  }
}

TEST_CASE("an empty histogram reports zeros and huge values are clamped") {
  LatencyHistogram histogram;
  CHECK(histogram.percentile(99) == 0);
  CHECK(histogram.min() == 0);
  histogram.record(UINT64_MAX);
  CHECK(histogram.max() == UINT64_MAX);
  CHECK(histogram.percentile(50) == LatencyHistogram::max_trackable);
}
