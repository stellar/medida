//
// Copyright (c) 2012 Daniel Lundin
//

#include "medida/histogram.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "medida/metrics_registry.h"

using namespace medida;

TEST(HistogramTest, anEmptyHistogram) {
  MetricsRegistry registry {};
  auto& histogram = registry.NewHistogram({"a", "b", "c"}, SamplingInterface::kUniform);

  EXPECT_EQ(0, histogram.count());
  EXPECT_EQ(0.0, histogram.max());
  EXPECT_EQ(0.0, histogram.min());
  EXPECT_EQ(0.0, histogram.mean());
  EXPECT_EQ(0.0, histogram.std_dev());
  EXPECT_EQ(0.0, histogram.sum());

  auto snapshot = histogram.GetSnapshot();
  EXPECT_EQ(0.0, snapshot.getMedian());
  EXPECT_EQ(0.0, snapshot.get75thPercentile());
  EXPECT_EQ(0.0, snapshot.get99thPercentile());
  EXPECT_EQ(0, snapshot.size());
}


TEST(HistogramTest, aHistogramWith1000Elements) {
  MetricsRegistry registry {};
  auto& histogram = registry.NewHistogram({"a", "b", "c"}, SamplingInterface::kUniform);

  for (auto i = 1; i <= 1000; i++) {
    histogram.Update(i);
  }

  EXPECT_EQ(1000, histogram.count());
  EXPECT_NEAR(1000.0, histogram.max(), 0.001);
  EXPECT_NEAR(1.0, histogram.min(), 0.001);
  EXPECT_NEAR(500.5, histogram.mean(), 0.001);
  EXPECT_NEAR(288.8194360957494, histogram.std_dev(), 0.001);
  EXPECT_NEAR(500500, histogram.sum(), 0.1);

  auto snapshot = histogram.GetSnapshot();
  EXPECT_NEAR(500.5, snapshot.getMedian(), 0.0001);
  EXPECT_NEAR(750.25, snapshot.get75thPercentile(), 0.0001);
  EXPECT_NEAR(990.00999999999999, snapshot.get99thPercentile(), 0.0001);
  EXPECT_EQ(1000, snapshot.size());
}

TEST(HistogramTest, ckmsWindowSize) {
  MetricsRegistry r1 {std::chrono::seconds(1)}, r2 {std::chrono::seconds(2000000000)};
  auto& histogram1 = r1.NewHistogram({"a", "b", "c"}, SamplingInterface::kCKMS);
  auto& histogram2 = r2.NewHistogram({"a", "b", "c"}, SamplingInterface::kCKMS);

  auto updateValues = [&]() {
    histogram1.Update(123);
    histogram2.Update(123);
  };

  updateValues();

  // CKMS reports the previous window.
  // The value 123 was added in the current window,
  // so we shouldn't report anything yet.
  EXPECT_EQ(0u, histogram1.GetSnapshot().size());
  EXPECT_EQ(0u, histogram2.GetSnapshot().size());

  auto snapshot1_size = std::uint64_t {0};
  for (auto attempt = 0; attempt < 3 && snapshot1_size == 0; attempt++) {
    if (attempt != 0) {
      updateValues();
    }

    auto deadline = Clock::now() + std::chrono::seconds(2);
    while (true) {
      auto snapshot1 = histogram1.GetSnapshot();
      auto snapshot2 = histogram2.GetSnapshot();
      // histogram2 snapshot will always be empty
      EXPECT_EQ(0u, snapshot2.size());

      snapshot1_size = snapshot1.size();
      if (snapshot1_size != 0 || Clock::now() >= deadline) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  // Since r1 uses 1 second as the window size,
  // the value 123 must eventually be in the previous window.
  // r1 uses 2000000000 seconds (= approx. 63 years) as the window size,
  // so the value 123 must not be in the current window yet.
  EXPECT_NE(0u, snapshot1_size);
  EXPECT_EQ(0u, histogram2.GetSnapshot().size());
}

TEST(HistogramTest, ckmsMetrics) {
  MetricsRegistry r {std::chrono::seconds(1)};
  auto& h = r.NewHistogram({"a", "b", "c"}, SamplingInterface::kCKMS);

  for (int i = 1; i <= 7; i++) {
      h.Update(i);
  }

  auto s = h.GetSnapshot();

  EXPECT_EQ(1, h.min());
  EXPECT_EQ(7, h.max());
  EXPECT_NEAR(2.1602468994693, h.std_dev(), 1e-6);
  EXPECT_EQ(28, h.sum());
  EXPECT_EQ(7, h.count());
}

TEST(HistogramTest, logHistogramMetrics) {
  MetricsRegistry r {std::chrono::seconds(1)};
  auto& h = r.NewHistogram({"a", "b", "c"}, SamplingInterface::kLogHistogram);

  for (int i = 1; i <= 7; i++) {
      h.Update(i);
  }

  EXPECT_EQ(1, h.min());
  EXPECT_EQ(7, h.max());
  EXPECT_NEAR(2.1602468994693, h.std_dev(), 1e-6);
  EXPECT_EQ(28, h.sum());
  EXPECT_EQ(7, h.count());
}

TEST(HistogramTest, updateManyIsSafeWithConcurrentReaders) {
  MetricsRegistry registry {};
  auto& histogram = registry.NewHistogram({"a", "b", "c"}, SamplingInterface::kUniform);
  const auto numThreads = 8;
  const auto batchesPerThread = 128;
  const auto valuesPerBatch = 16;
  const auto expectedCount = numThreads * batchesPerThread * valuesPerBatch;
  const auto expectedSum = expectedCount * (expectedCount + 1) / 2;
  std::atomic<bool> writersDone {false};
  bool observedInvalidState {false};
  bool observedNonEmptyHistogram {false};
  std::uint64_t readerObservations {0};
  std::vector<std::thread> threads;

  auto reader = std::thread {[&]() {
    while (!writersDone.load()) {
      auto count = histogram.count();
      auto sum = histogram.sum();
      auto min = histogram.min();
      auto max = histogram.max();
      auto mean = histogram.mean();
      auto stdDev = histogram.std_dev();
      auto snapshot = histogram.GetSnapshot();
      if (count != 0) {
        observedNonEmptyHistogram = true;
        if (sum < count || max < min || !std::isfinite(mean) ||
            !std::isfinite(stdDev) || snapshot.size() > expectedCount) {
          observedInvalidState = true;
        }
      }
      readerObservations++;
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }};

  for (auto threadIndex = 0; threadIndex < numThreads; threadIndex++) {
    threads.emplace_back([&, threadIndex]() {
      for (auto batchIndex = 0; batchIndex < batchesPerThread; batchIndex++) {
        std::vector<std::int64_t> values;
        values.reserve(valuesPerBatch);
        for (auto valueIndex = 0; valueIndex < valuesPerBatch; valueIndex++) {
          values.push_back(threadIndex * batchesPerThread * valuesPerBatch +
                           batchIndex * valuesPerBatch + valueIndex + 1);
        }
        histogram.UpdateMany(values);
        if ((batchIndex + threadIndex) % 2 == 0) {
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
  writersDone.store(true);
  reader.join();

  EXPECT_GE(readerObservations, 10u);
  EXPECT_TRUE(observedNonEmptyHistogram);
  EXPECT_FALSE(observedInvalidState);
  EXPECT_EQ(expectedCount, histogram.count());
  EXPECT_NEAR(1.0, histogram.min(), 0.001);
  EXPECT_NEAR(expectedCount, histogram.max(), 0.001);
  EXPECT_NEAR(expectedCount / 2.0 + 0.5, histogram.mean(), 0.001);
  EXPECT_NEAR(expectedSum, histogram.sum(), 0.1);
}
