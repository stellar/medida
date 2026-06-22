//
// Copyright (c) 2012 Daniel Lundin
//

#include "medida/meter.h"

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "medida/metrics_registry.h"

using namespace medida;


TEST(MeterTest, aBlankMeter) {
  Meter meter {"things"};
  EXPECT_EQ("things", meter.event_type());
  EXPECT_EQ(0u, meter.count());
  EXPECT_NEAR(0.0, meter.mean_rate(), 0.001);
}


TEST(MeterTest, createFromRegistry) {
  MetricsRegistry registry {};
  auto& meter = registry.NewMeter({"a", "b", "c"}, "things");
  EXPECT_EQ(0u, meter.count());
  EXPECT_EQ("things", meter.event_type());
}


TEST(MeterTest, aMeterWithThreeEvents) {
  Meter meter {"things"};
  meter.Mark(3);
  EXPECT_EQ(3u, meter.count());
}


TEST(MeterTest, meterTiming) {
  Meter meter {"things"};
  for (auto i = 0; i < 10; i++) {
    meter.Mark();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  EXPECT_EQ(10u, meter.count());
  EXPECT_NEAR(10, meter.mean_rate(), 0.5);
}


TEST(MeterTest, concurrentMarkAndRead) {
  Meter meter {"things"};
  std::atomic<bool> done {false};
  std::atomic<bool> valid {true};
  std::vector<std::thread> threads;

  for (auto i = 0; i < 4; i++) {
    threads.emplace_back([&]() {
      for (auto j = 0; j < 10000; j++) {
        meter.Mark();
      }
    });
  }

  std::thread reader {[&]() {
    while (!done.load()) {
      auto mean_rate = meter.mean_rate();
      auto one_minute_rate = meter.one_minute_rate();
      auto five_minute_rate = meter.five_minute_rate();
      auto fifteen_minute_rate = meter.fifteen_minute_rate();
      if (!std::isfinite(mean_rate) || mean_rate < 0.0 ||
          !std::isfinite(one_minute_rate) || one_minute_rate < 0.0 ||
          !std::isfinite(five_minute_rate) || five_minute_rate < 0.0 ||
          !std::isfinite(fifteen_minute_rate) || fifteen_minute_rate < 0.0) {
        valid.store(false);
      }
    }
  }};

  for (auto& thread : threads) {
    thread.join();
  }
  done.store(true);
  reader.join();

  EXPECT_TRUE(valid.load());
  EXPECT_EQ(40000u, meter.count());
}


TEST(MeterTest, concurrentClearAndMark) {
  Meter meter {"things"};
  std::atomic<bool> done {false};
  std::atomic<bool> valid {true};
  std::vector<std::thread> threads;

  for (auto i = 0; i < 4; i++) {
    threads.emplace_back([&]() {
      while (!done.load()) {
        meter.Mark();
      }
    });
  }

  std::thread reader {[&]() {
    while (!done.load()) {
      auto mean_rate = meter.mean_rate();
      auto one_minute_rate = meter.one_minute_rate();
      auto five_minute_rate = meter.five_minute_rate();
      auto fifteen_minute_rate = meter.fifteen_minute_rate();
      if (!std::isfinite(mean_rate) || mean_rate < 0.0 ||
          !std::isfinite(one_minute_rate) || one_minute_rate < 0.0 ||
          !std::isfinite(five_minute_rate) || five_minute_rate < 0.0 ||
          !std::isfinite(fifteen_minute_rate) || fifteen_minute_rate < 0.0) {
        valid.store(false);
      }
    }
  }};

  for (auto i = 0; i < 100; i++) {
    meter.Clear();
  }
  done.store(true);

  reader.join();

  for (auto& thread : threads) {
    thread.join();
  }

  meter.Clear();
  EXPECT_TRUE(valid.load());
  EXPECT_EQ(0u, meter.count());
  EXPECT_NEAR(0.0, meter.mean_rate(), 0.001);
}

