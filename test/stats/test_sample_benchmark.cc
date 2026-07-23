#include "medida/stats/ckms_sample.h"
#include "medida/stats/log_histogram_sample.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

using namespace medida::stats;

namespace {

struct Result {
  std::string name;
  std::size_t samples;
  std::chrono::nanoseconds elapsed;
};

struct LatencyResult {
  std::string name;
  std::size_t samples;
  std::chrono::nanoseconds min;
  std::chrono::nanoseconds median;
  std::chrono::nanoseconds p99;
  std::chrono::nanoseconds max;
  std::chrono::nanoseconds total;
};

std::vector<std::int64_t> MakeValues(std::size_t samples) {
  std::vector<std::int64_t> values;
  values.reserve(samples);

  std::uint64_t x = 0x123456789abcdefULL;
  for (std::size_t i = 0; i < samples; ++i) {
    x = x * 2862933555777941757ULL + 3037000493ULL;
    values.push_back(static_cast<std::int64_t>((x >> 16) % 1000000));
  }
  return values;
}

template <typename Fn>
Result Measure(const std::string& name, std::size_t samples, Fn fn) {
  auto start = std::chrono::steady_clock::now();
  fn();
  auto end = std::chrono::steady_clock::now();
  return {name, samples, std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)};
}

void PrintResult(const Result& result) {
  auto ns_per_sample = static_cast<double>(result.elapsed.count()) /
                       static_cast<double>(result.samples);
  auto samples_per_second = 1e9 / ns_per_sample;

  std::cerr << std::left << std::setw(36) << result.name << "  "
            << std::right << std::setw(12) << result.elapsed.count() / 1000000.0
            << " ms  " << std::setw(10) << ns_per_sample << " ns/update  "
            << std::setw(12) << samples_per_second << " updates/s\n";
}

void PrintLatencyResult(const LatencyResult& result) {
  auto mean = static_cast<double>(result.total.count()) /
              static_cast<double>(result.samples);

  std::cerr << std::left << std::setw(28) << result.name << "  "
            << std::right << "min " << std::setw(8) << result.min.count()
            << " ns  median " << std::setw(8) << result.median.count()
            << " ns  p99 " << std::setw(8) << result.p99.count()
            << " ns  max " << std::setw(10) << result.max.count()
            << " ns  mean " << std::setw(10) << mean << " ns\n";
}

template <typename Sample>
void UpdateWithTimestamp(Sample& sample, const std::vector<std::int64_t>& values,
                         medida::SystemClock::time_point timestamp) {
  for (auto value : values) {
    sample.Update(value, timestamp);
  }
}

template <typename Sample>
void UpdateWithClock(Sample& sample, const std::vector<std::int64_t>& values) {
  for (auto value : values) {
    sample.Update(value);
  }
}

template <typename Sample>
LatencyResult MeasureUpdateLatencies(const std::string& name, Sample& sample,
                                     const std::vector<std::int64_t>& values,
                                     medida::SystemClock::time_point timestamp) {
  std::vector<std::chrono::nanoseconds> timings;
  timings.reserve(values.size());
  auto total = std::chrono::nanoseconds{0};

  for (auto value : values) {
    auto start = std::chrono::steady_clock::now();
    sample.Update(value, timestamp);
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);
    timings.push_back(elapsed);
    total += elapsed;
  }

  std::sort(timings.begin(), timings.end());
  auto p99_index = static_cast<std::size_t>(values.size() * 99 / 100);
  if (p99_index >= timings.size()) {
    p99_index = timings.size() - 1;
  }

  return {name,
          values.size(),
          timings.front(),
          timings[timings.size() / 2],
          timings[p99_index],
          timings.back(),
          total};
}

template <typename Sample>
Result MeasureContendedUpdates(const std::string& name, Sample& sample,
                               const std::vector<std::int64_t>& values,
                               std::size_t thread_count,
                               medida::SystemClock::time_point timestamp) {
  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  return Measure(name, values.size() * thread_count, [&] {
    for (std::size_t i = 0; i < thread_count; ++i) {
      threads.emplace_back([&] {
        UpdateWithTimestamp(sample, values, timestamp);
      });
    }
    for (auto& thread : threads) {
      thread.join();
    }
  });
}

} // namespace

TEST(SampleBenchmarkTest, DISABLED_updateThroughput) {
  auto const samples = std::size_t{1000000};
  auto values = MakeValues(samples);
  auto timestamp = medida::SystemClock::time_point{};

  std::cerr << "\nSample update benchmark (" << samples << " values)\n";
  std::cerr << "Run manually with:\n"
            << "  ./build/test/test-medida --gtest_also_run_disabled_tests "
            << "--gtest_filter=SampleBenchmarkTest.DISABLED_updateThroughput\n\n";

  {
    CKMSSample sample;
    auto result = Measure("CKMS Update(value, timestamp)", samples, [&] {
      UpdateWithTimestamp(sample, values, timestamp);
    });
    EXPECT_EQ(samples, sample.MakeSnapshot(timestamp + std::chrono::seconds(30)).size());
    PrintResult(result);
  }

  {
    LogHistogramSample sample;
    auto result = Measure("LogHistogram Update(value, timestamp)", samples, [&] {
      UpdateWithTimestamp(sample, values, timestamp);
    });
    EXPECT_EQ(samples, sample.MakeSnapshot(timestamp + std::chrono::seconds(30)).size());
    PrintResult(result);
  }

  {
    CKMSSample sample;
    auto result = Measure("CKMS Update(value)", samples, [&] {
      UpdateWithClock(sample, values);
    });
    EXPECT_EQ(samples, sample.MakeSnapshot(medida::SystemClock::now() + std::chrono::seconds(30)).size());
    PrintResult(result);
  }

  {
    LogHistogramSample sample;
    auto result = Measure("LogHistogram Update(value)", samples, [&] {
      UpdateWithClock(sample, values);
    });
    EXPECT_EQ(samples, sample.MakeSnapshot(medida::SystemClock::now() + std::chrono::seconds(30)).size());
    PrintResult(result);
  }

  {
    CKMSSample sample;
    auto result = Measure("CKMS UpdateMany(values, timestamp)", samples, [&] {
      sample.UpdateMany(values, timestamp);
    });
    EXPECT_EQ(samples, sample.MakeSnapshot(timestamp + std::chrono::seconds(30)).size());
    PrintResult(result);
  }

  {
    LogHistogramSample sample;
    auto result = Measure("LogHistogram UpdateMany(values, timestamp)", samples, [&] {
      sample.UpdateMany(values, timestamp);
    });
    EXPECT_EQ(samples, sample.MakeSnapshot(timestamp + std::chrono::seconds(30)).size());
    PrintResult(result);
  }
}

TEST(SampleBenchmarkTest, DISABLED_contendedUpdateThroughput) {
  auto const thread_count = std::size_t{8};
  auto const samples_per_thread = std::size_t{250000};
  auto const total_samples = thread_count * samples_per_thread;
  auto values = MakeValues(samples_per_thread);
  auto timestamp = medida::SystemClock::time_point{};

  std::cerr << "\nContended sample update benchmark (" << thread_count
            << " threads, " << samples_per_thread << " values/thread)\n";
  std::cerr << "Run manually with:\n"
            << "  ./build/test/test-medida --gtest_also_run_disabled_tests "
            << "--gtest_filter=SampleBenchmarkTest.DISABLED_contendedUpdateThroughput\n\n";

  {
    CKMSSample sample;
    auto result = MeasureContendedUpdates("CKMS contended Update(timestamp)",
                                          sample, values, thread_count,
                                          timestamp);
    EXPECT_EQ(total_samples,
              sample.MakeSnapshot(timestamp + std::chrono::seconds(30)).size());
    PrintResult(result);
  }

  {
    LogHistogramSample sample;
    auto result = MeasureContendedUpdates("LogHistogram contended Update(timestamp)",
                                          sample, values, thread_count,
                                          timestamp);
    EXPECT_EQ(total_samples,
              sample.MakeSnapshot(timestamp + std::chrono::seconds(30)).size());
    PrintResult(result);
  }
}

TEST(SampleBenchmarkTest, DISABLED_updateLatencySpikes) {
  auto const samples = std::size_t{200000};
  auto values = MakeValues(samples);
  auto timestamp = medida::SystemClock::time_point{};

  std::cerr << "\nPer-update latency benchmark (" << samples << " values)\n";
  std::cerr << "Run manually with:\n"
            << "  ./build/test/test-medida --gtest_also_run_disabled_tests "
            << "--gtest_filter=SampleBenchmarkTest.DISABLED_updateLatencySpikes\n\n";

  {
    CKMSSample sample;
    auto result = MeasureUpdateLatencies("CKMS Update(timestamp)", sample,
                                         values, timestamp);
    EXPECT_EQ(samples, sample.MakeSnapshot(timestamp + std::chrono::seconds(30)).size());
    PrintLatencyResult(result);
  }

  {
    LogHistogramSample sample;
    auto result = MeasureUpdateLatencies("LogHistogram Update(timestamp)", sample,
                                         values, timestamp);
    EXPECT_EQ(samples, sample.MakeSnapshot(timestamp + std::chrono::seconds(30)).size());
    PrintLatencyResult(result);
  }
}