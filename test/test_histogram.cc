//
// Copyright (c) 2012 Daniel Lundin
//

#include "medida/histogram.h"

#include <gtest/gtest.h>
#include <thread>

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

  histogram1.Update(123);
  histogram2.Update(123);

  // CKMS reports the previous window.
  // The value 123 was added in the current window,
  // so we shouldn't report anything yet.
  EXPECT_EQ(0, histogram1.GetSnapshot().size());
  EXPECT_EQ(0, histogram2.GetSnapshot().size());

  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Since r1 uses 1 second as the window size,
  // the value 123 must be in the previous window now.
  // r1 uses 2000000000 seconds (= approx. 63 years) as the window size,
  // so the value 123 must not be in the current window yet.
  EXPECT_EQ(1, histogram1.GetSnapshot().size());
  EXPECT_EQ(0, histogram2.GetSnapshot().size());
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
