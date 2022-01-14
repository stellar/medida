// This file is originally from the Prometheus project, with
// local modifications made by Stellar Development Foundation.
//
// Copyright (c) 2016-2019 Jupp Mueller
// Copyright (c) 2017-2019 Gregor Jasny
//
// And many contributors, see
// https://github.com/jupp0r/prometheus-cpp/graphs/contributors
//
// Licensed under MIT license.
// https://opensource.org/licenses/MIT

#include "medida/stats/ckms.h"


#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace medida {
namespace stats {

// The default quantiles request the error be less than 0.1% (=0.001) for P99 and P50.
std::vector<CKMS::Quantile> const kDefaultQuantiles = {{0.99, 0.001}, {0.5, 0.001}};

CKMS::CKMS() : CKMS(kDefaultQuantiles) {
}

std::size_t CKMS::count() const {
    return count_ + buffer_count_;
}

double CKMS::max() const {
    return max_;
}

CKMS::Quantile::Quantile(double quantile, double error)
    : quantile(quantile),
      error(error),
      u(2.0 * error / (1.0 - quantile)),
      v(2.0 * error / quantile) {}

CKMS::Item::Item(double value, int lower_delta, int delta)
    : value(value), g(lower_delta), delta(delta) {}


CKMS::CKMS(const std::vector<Quantile>& quantiles)
    : quantiles_(quantiles), count_(0), buffer_{}, buffer_count_(0), size_when_last_sorted_(0) {}

void CKMS::insert(double value) {
  if (count() == 0) {
      max_ = value;
  } else {
      max_ = std::max(max_, value);
  }

  buffer_[buffer_count_] = value;
  ++buffer_count_;

  if (buffer_count_ == buffer_.size()) {
    insertBatch();
    compress();
  }
}

double CKMS::get(double q) {
  if (count() < buffer_.size()) {
      // in this block, count() == buffer_count_ as we've accumulated less
      // than buffer_.size() samples
      if (buffer_count_ == 0)
      {
          return 0.0;
      }
      // The sample size is still very small.
      // We will calculate the exact value.
      if (size_when_last_sorted_ < buffer_count_) {
          // We've added more elements since we last sorted.
          // We need to sort again.
          // This means, in total, we may sort this array buffer_count_ times.
          // Therefore, in the worst case scenario,
          // sorting will cost us O(buffer_count_ * buffer_count_ * log(buffer_count_)) operations.
          std::sort(buffer_.begin(), buffer_.begin() + buffer_count_);
          size_when_last_sorted_ = buffer_count_;
      }
      if (q <= 0 || 1.0 < q) {
          // Invalid q.
          return 0.0;
      } else {
          // We want to find x such that
          // x is the smallest number in the given sample set such that
          // at least q% of all samples are <= x.
          // In other words, we want ceil(buffer_count_ * q) elements
          // to be <= x.
          // Then we want ceil(buffer_count_ * q)-th element,
          // whose index is ceil(buffer_count_ * q) - 1 since
          // the index starts at 0.
          return buffer_[int(ceil(buffer_count_ * q)) - 1];
      }
  }

  insertBatch();
  compress();

  if (sample_.empty()) {
    return 0;
  }

  int rankMin = 0;
  const auto desired = static_cast<int>(q * count_);
  const auto bound = desired + (allowableError(desired) / 2);

  auto it = sample_.begin();
  decltype(it) prev;
  auto cur = it++;

  while (it != sample_.end()) {
    prev = cur;
    cur = it++;

    rankMin += prev->g;

    if (rankMin + cur->g + cur->delta > bound) {
      return prev->value;
    }
  }

  return sample_.back().value;
}

void CKMS::reset() {
  count_ = 0;
  sample_.clear();
  buffer_count_ = 0;
  max_ = 0;
  size_when_last_sorted_ = 0;
}

double CKMS::allowableError(int rank) {
  auto size = sample_.size();
  double minError = size + 1;

  for (const auto& q : quantiles_.get()) {
    double error;
    if (rank <= q.quantile * size) {
      error = q.u * (size - rank);
    } else {
      error = q.v * rank;
    }
    if (error < minError) {
      minError = error;
    }
  }

  return minError;
}

bool CKMS::insertBatch() {
  if (buffer_count_ == 0) {
    return false;
  }

  std::sort(buffer_.begin(), buffer_.begin() + buffer_count_);

  std::size_t start = 0;
  if (sample_.empty()) {
    sample_.emplace_back(buffer_[0], 1, 0);
    ++start;
    ++count_;
  }

  std::size_t idx = 0;
  std::size_t item = idx++;

  for (std::size_t i = start; i < buffer_count_; ++i) {
    double v = buffer_[i];
    while (idx < sample_.size() && sample_[item].value < v) {
      item = idx++;
    }

    if (sample_[item].value > v) {
      --idx;
    }

    int delta;
    if (idx - 1 == 0 || idx + 1 == sample_.size()) {
      delta = 0;
    } else {
      delta = static_cast<int>(std::floor(allowableError(idx + 1))) + 1;
    }

    sample_.emplace(sample_.begin() + idx, v, 1, delta);
    count_++;
    item = idx++;
  }

  buffer_count_ = 0;
  return true;
}

void CKMS::compress() {
  if (sample_.size() < 2) {
    return;
  }

  std::size_t idx = 0;
  std::size_t prev;
  std::size_t next = idx++;

  while (idx < sample_.size()) {
    prev = next;
    next = idx++;

    if (sample_[prev].g + sample_[next].g + sample_[next].delta <=
        allowableError(idx - 1)) {
      sample_[next].g += sample_[prev].g;
      sample_.erase(sample_.begin() + prev);
    }
  }
}

} // namespace stats
} // namespace medida
