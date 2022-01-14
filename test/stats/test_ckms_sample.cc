#include "medida/stats/ckms_sample.h"

#include <gtest/gtest.h>

using namespace medida::stats;

TEST(CKMSSampleTest, aSameValueEverySecond) {
  CKMSSample sample;

  auto t = medida::SystemClock::now();
  for (auto i = 0; i < 300; i++) {
    t += std::chrono::seconds(1);
    sample.Update(100, t);
  }

  EXPECT_EQ(30, sample.size(t));

  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(100, snapshot.getValue(0.5));
  EXPECT_EQ(100, snapshot.getValue(0.99));
  EXPECT_EQ(100, snapshot.getValue(1));
}

TEST(CKMSSampleTest, aThreeDifferentValues) {
  CKMSSample sample;

  auto t = medida::SystemClock::now();
  for (auto i = 0; i < 300; i++) {
    t += std::chrono::seconds(1);
    sample.Update(i % 3, t);
  }

  // We should only keep 30 seconds of data.
  EXPECT_EQ(30, sample.size(t));

  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(1, snapshot.getValue(0.5));
  EXPECT_EQ(2, snapshot.getValue(0.99));
  EXPECT_EQ(2, snapshot.getValue(1));
}

TEST(CKMSSampleTest, aCKMSSnapshotTestCurrentWindow) {
  CKMSSample sample;

  auto t = medida::Clock::time_point();

  // [0 seconds, 30 seconds) contains {1, 1, ..., 1}. (30 of them)
  // [30 seconds, 60 seconds) contains {2, 2, ..., 2}. (15 of them)
  for (auto i = 0; i < 45; i++) {
    if (i < 30) {
      sample.Update(1, t);
    } else {
      sample.Update(2, t);
    }
    t += std::chrono::seconds(1);
  }

  // t = 45 seconds since epoch.
  // Thus t is in the current window, and it should return the previous window.
  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(1, snapshot.getValue(0.5));
}

TEST(CKMSSampleTest, aCKMSSnapshotTestNextWindow) {
  CKMSSample sample;

  auto t = medida::Clock::time_point();

  // [0 seconds, 30 seconds) contains {1, 1, ..., 1}. (30 of them)
  for (auto i = 0; i < 30; i++) {
    sample.Update(1, t);
    t += std::chrono::seconds(1);
  }

  // t = 30 seconds since epoch.
  // Since t is past the current window (= {1, 1, ..., 1}),
  // we expect MakeSnapshot to return the current window.
  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(1, snapshot.getValue(0.5));
  EXPECT_EQ(30, snapshot.size());
}

TEST(CKMSSampleTest, aCKMSSnapshotTestFuture) {
  CKMSSample sample;

  auto t = medida::Clock::time_point();

  // [0 seconds, 30 seconds) contains {1, 1, ..., 1}. (30 of them)
  for (auto i = 0; i < 30; i++) {
    sample.Update(1, t);
    t += std::chrono::seconds(1);
  }

  t += std::chrono::seconds(100);

  // Since t is way past the current window (= {1, 1, ..., 1}),
  // we expect MakeSnapshot to return an empty window.
  auto snapshot = sample.MakeSnapshot(t);

  EXPECT_EQ(0, snapshot.size());
}

TEST(CKMSSampleTest, aCKMSUpdateWithHugeGap) {
  CKMSSample sample;

  auto t = medida::Clock::time_point();

  for (auto i = 0; i < 10; i++) {
    sample.Update(1, t);
  }

  t += std::chrono::seconds(100);
  sample.Update(10, t);
  sample.Update(10, t);

  t += std::chrono::seconds(30);

  // We expect that CKMSSample dropped all 1's when we added 10's
  // since so much time had passed.
  // Therefore, we expect the snapshot to only contain two 10's.
  auto snapshot = sample.MakeSnapshot(t);
  EXPECT_EQ(2, snapshot.size());
}
