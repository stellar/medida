#include "medida/stats/log_histogram_sample.h"

#include <gtest/gtest.h>
#include <limits>

using namespace medida::stats;

TEST(LogHistogramSampleTest, aSameValueEverySecond) {
  LogHistogramSample sample;

  auto t = medida::SystemClock::time_point();
  for (auto i = 0; i < 300; i++) {
    t += std::chrono::seconds(1);
    sample.Update(100, t);
  }

  EXPECT_EQ(30, sample.size(t));

  auto snapshot = sample.MakeSnapshot(t);
  EXPECT_EQ(30, snapshot.size());
  EXPECT_NEAR(100, snapshot.getValue(0.5), 5);
  EXPECT_NEAR(100, snapshot.getValue(0.99), 5);
  EXPECT_EQ(100, snapshot.getValue(1));
}

TEST(LogHistogramSampleTest, aThreeDifferentValues) {
  LogHistogramSample sample;

  auto t = medida::SystemClock::time_point();
  for (auto i = 0; i < 300; i++) {
    t += std::chrono::seconds(1);
    sample.Update(i % 3, t);
  }

  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(30, snapshot.size());
  EXPECT_EQ(1, snapshot.getValue(0.5));
  EXPECT_EQ(2, snapshot.getValue(0.99));
  EXPECT_EQ(2, snapshot.getValue(1));
}

TEST(LogHistogramSampleTest, aSnapshotReturnsPreviousWindow) {
  LogHistogramSample sample;

  auto t = medida::SystemClock::time_point();
  for (auto i = 0; i < 45; i++) {
    sample.Update(i < 30 ? 1 : 2, t);
    t += std::chrono::seconds(1);
  }

  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(30, snapshot.size());
  EXPECT_EQ(1, snapshot.getValue(0.5));
}

TEST(LogHistogramSampleTest, aSnapshotDropsHugeGaps) {
  LogHistogramSample sample;

  auto t = medida::SystemClock::time_point();
  for (auto i = 0; i < 10; i++) {
    sample.Update(1, t);
  }

  t += std::chrono::seconds(100);
  sample.Update(10, t);
  sample.Update(10, t);

  t += std::chrono::seconds(30);

  auto snapshot = sample.MakeSnapshot(t);
  EXPECT_EQ(2, snapshot.size());
  EXPECT_EQ(10, snapshot.getValue(1));
}

TEST(LogHistogramSampleTest, aSpikyInputs) {
  LogHistogramSample sample;

  auto t = medida::SystemClock::time_point();

  auto const size = 100000;
  for (auto i = 0; i < 5; i++) {
    t += std::chrono::seconds(30);
    for (int j = 1; j <= size; j++) {
      sample.Update(j, t);
    }
  }

  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(size, snapshot.size());
  EXPECT_NEAR(size * 0.5, snapshot.getValue(0.5), size * 0.05);
  EXPECT_NEAR(size * 0.99, snapshot.getValue(0.99), size * 0.05);
  EXPECT_EQ(size, snapshot.getValue(1));
}

TEST(LogHistogramSampleTest, supportsNegativeValues) {
  LogHistogramSample sample;
  auto t = medida::SystemClock::time_point();

  sample.Update(-100, t);
  sample.Update(-10, t);
  sample.Update(0, t);
  sample.Update(10, t);
  sample.Update(100, t);

  t += std::chrono::seconds(30);
  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(5, snapshot.size());
  EXPECT_LT(snapshot.getValue(0), -90);
  EXPECT_EQ(0, snapshot.getValue(0.5));
  EXPECT_EQ(100, snapshot.getValue(1));
}

TEST(LogHistogramSampleTest, requiresPowerOfTwoBucketsPerLevel) {
  EXPECT_THROW(LogHistogramSample(std::chrono::seconds(30), 15),
               std::invalid_argument);
  EXPECT_NO_THROW(LogHistogramSample(std::chrono::seconds(30), 16));
}

TEST(LogHistogramSampleTest, supportsCompactConfig) {
  LogHistogramSample::Config config;
  config.buckets_per_level = 8;
  config.max_absolute_value = 15;
  config.max_events_per_window = std::numeric_limits<std::uint16_t>::max();
  LogHistogramSample sample(config);

  auto t = medida::SystemClock::time_point();
  sample.Update(100, t);

  t += std::chrono::seconds(30);
  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(1, snapshot.size());
  EXPECT_LE(snapshot.getValue(0.5), 16);
  EXPECT_EQ(100, snapshot.max());
}

TEST(LogHistogramSampleTest, requiresPositiveMaxAbsoluteValue) {
  LogHistogramSample::Config config;
  config.max_absolute_value = 0;

  EXPECT_THROW(LogHistogramSample sample(config), std::invalid_argument);
}