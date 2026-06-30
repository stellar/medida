//
// Copyright (c) 2012 Daniel Lundin
//

#include "medida/timer.h"

#include <functional>
#include <iostream>
#include <thread>

#include <gtest/gtest.h>

#include "medida/metrics_registry.h"

using namespace medida;


struct TimerTest : public ::testing::Test {
  // Use 1 second as the size of the CKMS window
  Timer timer{std::chrono::milliseconds(1),
              std::chrono::seconds(1),
              std::chrono::seconds(1)};
};


TEST_F(TimerTest, hasDurationUnit) {
  EXPECT_EQ(std::chrono::milliseconds(1), timer.duration_unit());
}


TEST_F(TimerTest, hasRateUnit) {
  EXPECT_EQ(std::chrono::seconds(1), timer.rate_unit());
}


TEST_F(TimerTest, createFromRegistry) {
  MetricsRegistry registry {};
  auto& timer2 = registry.NewTimer({"a", "b", "c"});
  EXPECT_EQ(0, timer2.count());
}


TEST_F(TimerTest, aBlankTimer) {
  EXPECT_EQ(0, timer.count());
  EXPECT_NEAR(0.0, timer.min(), 0.001);
  EXPECT_NEAR(0.0, timer.max(), 0.001);
  EXPECT_NEAR(0.0, timer.mean(), 0.001);
  EXPECT_NEAR(0.0, timer.std_dev(), 0.001);
  EXPECT_NEAR(0.0, timer.mean_rate(), 0.001);
  EXPECT_NEAR(0.0, timer.one_minute_rate(), 0.001);
  EXPECT_NEAR(0.0, timer.five_minute_rate(), 0.001);
  EXPECT_NEAR(0.0, timer.fifteen_minute_rate(), 0.001);

  auto snapshot = timer.GetSnapshot();
  EXPECT_NEAR(0.0, snapshot.getMedian(), 0.001);
  EXPECT_NEAR(0.0, snapshot.get75thPercentile(), 0.001);
  EXPECT_NEAR(0.0, snapshot.get99thPercentile(), 0.001);
  EXPECT_EQ(0, snapshot.size());
}

// Take a snapshot of the next non-empty window (since window is based on the
// wall clock second bounds, the exact time to wait is non-deterministic).
stats::Snapshot takeSnapshot(Timer& timer, std::function<void()> repopulate = {}) {
  for (auto attempt = 0; attempt < 3; attempt++) {
    if (attempt != 0 && repopulate) {
      repopulate();
    }

    auto deadline = Clock::now() + std::chrono::seconds(2);
    while (true) {
      auto snapshot = timer.GetSnapshot();
      if (snapshot.size() != 0 || Clock::now() >= deadline) {
        if (snapshot.size() != 0 || !repopulate) {
          return snapshot;
        }
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  return timer.GetSnapshot();
}

TEST_F(TimerTest, timingASeriesOfEvents) {
  // As CKMS isn't very accurate when there's very few samples,
  // we will use for loops to add each value several times.
  auto updateValues = [&]() {
    for (int i = 0; i < 10; i++) {
      timer.Update(std::chrono::milliseconds(10));
      timer.Update(std::chrono::milliseconds(20));
      timer.Update(std::chrono::milliseconds(20));
      timer.Update(std::chrono::milliseconds(30));
      timer.Update(std::chrono::milliseconds(40));
    }
  };

  updateValues();

  EXPECT_EQ(50, timer.count());
  EXPECT_NEAR(10.0, timer.min(), 0.001);
  EXPECT_NEAR(40.0, timer.max(), 0.001);
  EXPECT_NEAR(24.0, timer.mean(), 0.001);
  EXPECT_NEAR(10.301575, timer.std_dev(), 0.001);


  auto snapshot = takeSnapshot(timer, updateValues);
  ASSERT_NE(0u, snapshot.size());
  EXPECT_NEAR(20.0, snapshot.getMedian(), 0.001);
  EXPECT_NEAR(30.0, snapshot.get75thPercentile(), 0.001);
  EXPECT_NEAR(40, snapshot.get99thPercentile(), 0.001);
  EXPECT_EQ(50, snapshot.size());
}


TEST_F(TimerTest, timingASeriesOfShortEvents) {
  // This test makes sure that the calculation and casting are done correctly,
  // and prevents short events from being ignored as rounding errors.
  auto updateValues = [&]() {
    for (int i = 0; i < 10; i++) {
      timer.Update(std::chrono::nanoseconds(1));
    }
  };

  updateValues();

  EXPECT_EQ(10, timer.count());
  EXPECT_NEAR(1e-6, timer.min(), 1e-9);
  EXPECT_NEAR(1e-6, timer.max(), 1e-9);
  EXPECT_NEAR(1e-6, timer.mean(), 1e-9);
  EXPECT_NEAR(0, timer.std_dev(), 1e-9);

  auto snapshot = takeSnapshot(timer, updateValues);
  EXPECT_NEAR(1e-6, snapshot.getMedian(), 1e-9);
  EXPECT_NEAR(1e-6, snapshot.get75thPercentile(), 1e-9);
  EXPECT_NEAR(1e-6, snapshot.get99thPercentile(), 1e-9);
  EXPECT_EQ(10, snapshot.size());
}


TEST_F(TimerTest, timingVariantValues) {
  timer.Update(std::chrono::nanoseconds(9223372036854775807));  // INT64_MAX
  timer.Update(std::chrono::nanoseconds(0));
  EXPECT_NEAR(6.521908912666392E12, timer.std_dev(), 0.001);
}


TEST_F(TimerTest, timerTimeScope) {
  {
    auto t = timer.TimeScope();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  {
    auto t = timer.TimeScope();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  EXPECT_EQ(2, timer.count());

  EXPECT_GE(timer.mean(), 150.0);
}


void my_func() {
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}


TEST_F(TimerTest, timerTimeFunction) {
  timer.Time(my_func);  
  EXPECT_EQ(1, timer.count());
  EXPECT_GE(timer.mean(), 100.0);
}


TEST_F(TimerTest, timerTimeLambda) {
  timer.Time([]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  });
  EXPECT_EQ(1, timer.count());
  EXPECT_GE(timer.mean(), 100.0);
}
