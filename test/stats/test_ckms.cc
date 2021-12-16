#include "medida/stats/ckms.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <random>

using namespace medida::stats;

TEST(CKMSTest, aCKMSAddHundredOnes) {
  std::vector<CKMS::Quantile> v({{0.5, 0.001}, {0.99, 0.001}, {1, 0}});
  auto ckms = CKMS(v);
  for (int i = 0; i < 100; i++) {
      ckms.insert(1);
  }
  EXPECT_NEAR(1, ckms.get(0.5), 1e-6);
  EXPECT_NEAR(1, ckms.get(0.99), 1e-6);
  EXPECT_NEAR(1, ckms.get(1), 1e-6);
}

TEST(CKMSTest, aCKMSAddOneToHundredThounsand) {
  // 0.1% error
  //
  // E.g., when guessing P99, it returns a value between
  // - P(1 - 0.001) * 99 = P98.901, and
  // - P(1 + 0.001) * 99 = P99.099
  //
  // See the definition of \epsilon-approximate in
  // http://dimacs.rutgers.edu/~graham/pubs/papers/bquant-icde.pdf
  double const error = 0.001;


  auto const percentiles = {0.5, 0.75, 0.9, 0.99};
  std::vector<CKMS::Quantile> v;
  v.reserve(percentiles.size());
  for (auto const q: percentiles) {
    v.push_back({q, error});
  }

  auto ckms = CKMS(v);

  int const count = 100 * 1000;
  for (int i = 1; i <= count; i++) {
      ckms.insert(i);
  }

  for (auto const q: percentiles) {
      EXPECT_LE((1 - error) * q * count, ckms.get(q));
      EXPECT_GE((1 + error) * q * count, ckms.get(q));
  }
}

TEST(CKMSTest, aCKMSUniform) {
  double const error = 0.001;
  auto const percentiles = {0.5, 0.75, 0.9, 0.99};
  std::vector<CKMS::Quantile> v;
  v.reserve(percentiles.size());
  for (auto const q: percentiles) {
    v.push_back({q, error});
  }

  auto ckms = CKMS(v);

  srand(time(NULL));
  int const count = 100 * 1000;
  std::vector<int> values;
  values.reserve(count);
  for (int i = 1; i <= count; i++) {
      auto x = rand();
      values.push_back(x);
      ckms.insert(x);
  }

  std::sort(values.begin(), values.end());
  for (auto const q: percentiles) {
      EXPECT_LE(values[int((1 - error) * q * count)], ckms.get(q));
      EXPECT_GE(values[int((1 + error) * q * count)], ckms.get(q));
  }
}

TEST(CKMSTest, aCKMSGamma) {
  double const error = 0.001;
  auto const percentiles = {0.5, 0.75, 0.9, 0.99};
  std::vector<CKMS::Quantile> v;
  v.reserve(percentiles.size());
  for (auto const q: percentiles) {
    v.push_back({q, error});
  }

  auto ckms = CKMS(v);

  int const count = 100 * 1000;
  std::vector<double> values;
  values.reserve(count);

  // 0 = seed
  std::mt19937 gen(0);

  // A gamma distribution with alpha=20, and beta=100
  // gives a bell curve with the top ~2000 between ~800 and ~400.
  std::gamma_distribution<double> d(20, 100);
  for (int i = 0; i < count; i++) {
      auto x = d(gen);
      values.push_back(x);
      ckms.insert(x);
  }

  std::sort(values.begin(), values.end());
  for (auto const q: percentiles) {
      EXPECT_LE(values[int((1 - error) * q * count)], ckms.get(q));
      EXPECT_GE(values[int((1 + error) * q * count)], ckms.get(q));
  }
}
