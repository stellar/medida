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
  return allowableError(rank, sample_.size());
}

double CKMS::allowableError(int rank, std::size_t size) const {
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

  // Single-pass merge of the sorted buffer into the sorted sample. This is
  // semantically identical to the historical version (which emplaced each
  // value mid-vector, costing O(buffer * sample) element moves per batch and
  // causing latency spikes on hot histograms), including its quirks:
  //  - a value equal to an existing element is inserted right after the
  //    first such element encountered by the forward scan;
  //  - delta is computed from the insertion *position* in the evolving
  //    vector and the evolving (pre-insertion) size, with delta = 0 only for
  //    position 1 and the second-to-last position (position 0 falls through
  //    to the general formula due to size_t underflow in `idx - 1 == 0`).
  //
  // Scan state: `cur` is the element the scan pointer rests on (either an
  // original sample element or the most recently inserted value); elements
  // before it are in `merged`; elements after it are sample_[s..n-1].
  std::size_t const n = sample_.size();
  std::vector<Item> merged;
  merged.reserve(n + buffer_count_ - start);

  Item cur = sample_[0];
  std::size_t s = 1;

  for (std::size_t i = start; i < buffer_count_; ++i) {
    double v = buffer_[i];
    while (cur.value < v && s < n) {
      merged.push_back(cur);
      cur = sample_[s++];
    }

    // Pre-insertion size of the evolving vector: merged + cur + suffix.
    std::size_t evolvingSize = merged.size() + 1 + (n - s);
    std::size_t idx;
    if (cur.value > v) {
      // Insert before cur: cur (always an original sample element here,
      // since previously inserted values are <= v) returns to the head of
      // the unconsumed suffix.
      idx = merged.size();
      --s;
    } else {
      // cur.value <= v: insert right after cur. This covers both the
      // equal-value case and the append-at-end case where the scan
      // consumed the whole suffix.
      merged.push_back(cur);
      idx = merged.size();
    }

    int delta;
    if (idx - 1 == 0 || idx + 1 == evolvingSize) {
      delta = 0;
    } else {
      delta = static_cast<int>(std::floor(
                  allowableError(static_cast<int>(idx + 1), evolvingSize))) +
              1;
    }

    cur = Item(v, 1, delta);
    count_++;
  }

  merged.push_back(cur);
  merged.insert(merged.end(), sample_.begin() + s, sample_.end());
  sample_.swap(merged);

  buffer_count_ = 0;
  return true;
}

void CKMS::compress() {
  if (sample_.size() < 2) {
    return;
  }

  // Walk adjacent (prev, next) pairs, folding prev into next when the
  // combined error stays within bounds. Semantically identical to the
  // historical version (which did an O(n) vector::erase per fold),
  // including its index bookkeeping quirks: a freshly folded item is not
  // considered as `prev` for the following pair, and the error allowance is
  // computed from next's position in (and size of) the evolving vector.
  std::size_t const n = sample_.size();
  std::vector<Item> out;
  out.reserve(n);

  Item prev = sample_[0];
  bool havePrev = true;
  std::size_t i = 1;
  while (i < n) {
    Item const& next = sample_[i];
    std::size_t evolvingSize = out.size() + 1 + (n - i);
    if (prev.g + next.g + next.delta <=
        allowableError(static_cast<int>(out.size() + 1), evolvingSize)) {
      Item folded = next;
      folded.g += prev.g;
      out.push_back(folded);
      ++i;
      if (i < n) {
        prev = sample_[i];
        ++i;
      } else {
        havePrev = false;
      }
    } else {
      out.push_back(prev);
      prev = next;
      ++i;
    }
  }
  if (havePrev) {
    out.push_back(prev);
  }
  sample_.swap(out);
}

} // namespace stats
} // namespace medida
