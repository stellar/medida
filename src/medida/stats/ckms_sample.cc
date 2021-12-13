// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "medida/stats/ckms_sample.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <cassert>

namespace medida {
namespace stats {

class CKMSSample::Impl {
 public:
  Impl(std::chrono::seconds = std::chrono::seconds(30));
  ~Impl();
  void Clear();
  std::uint64_t size();
  std::uint64_t size(Clock::time_point timestamp);
  std::uint64_t sum();
  std::uint64_t min();
  std::uint64_t max();
  std::uint64_t variance();
  void Update(std::int64_t value);
  void Update(std::int64_t value, Clock::time_point timestamp);
  Snapshot MakeSnapshot();
  Snapshot MakeSnapshot(Clock::time_point timestamp);
 private:
  std::mutex mutex_;
  std::shared_ptr<CKMS> prev_window_, cur_window_;
  mutable Clock::time_point last_asserted_time_;
  Clock::time_point cur_window_begin_;
  std::chrono::seconds window_size_;
  Clock::time_point CalculateCurrentWindowStartingPoint(Clock::time_point time) const;
  void AssertValidTime(Clock::time_point const& timestamp) const;
  bool IsInCurrentWindow(Clock::time_point const& timestamp) const;
  bool IsInNextWindow(Clock::time_point const& timestamp) const;
};

CKMSSample::CKMSSample(std::chrono::seconds window_size) : impl_ {new CKMSSample::Impl {window_size}} {
}


CKMSSample::~CKMSSample() {
}

void CKMSSample::Clear() {
  impl_->Clear();
}

double CKMSSample::min() const {
  return impl_->min();
}

double CKMSSample::max() const {
  return impl_->max();
}

double CKMSSample::variance() const {
  return impl_->variance();
}

double CKMSSample::sum() const {
  return impl_->sum();
}

std::uint64_t CKMSSample::size() const {
  return impl_->size();
}

std::uint64_t CKMSSample::size(Clock::time_point timestamp) const {
  return impl_->size(timestamp);
}

void CKMSSample::Update(std::int64_t value) {
  impl_->Update(value);
}

void CKMSSample::Update(std::int64_t value, Clock::time_point timestamp) {
  impl_->Update(value, timestamp);
}

Snapshot CKMSSample::MakeSnapshot() const {
  return impl_->MakeSnapshot();
}

Snapshot CKMSSample::MakeSnapshot(Clock::time_point timestamp) const {
  return impl_->MakeSnapshot(timestamp);
}

// === Implementation ===

Clock::time_point CKMSSample::Impl::CalculateCurrentWindowStartingPoint(Clock::time_point time) const {
    return time - (std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()) % window_size_);
}

void CKMSSample::Impl::AssertValidTime(Clock::time_point const& timestamp) const {
    assert(last_asserted_time_ <= timestamp);
    last_asserted_time_ = timestamp;
}

bool CKMSSample::Impl::IsInCurrentWindow(Clock::time_point const& timestamp) const {
    return cur_window_begin_ <= timestamp && timestamp < cur_window_begin_ + window_size_;
}

bool CKMSSample::Impl::IsInNextWindow(Clock::time_point const& timestamp) const {
    return cur_window_begin_ + window_size_ <= timestamp && timestamp < cur_window_begin_ + 2 * window_size_;
}

CKMSSample::Impl::Impl(std::chrono::seconds window_size) :
    prev_window_(std::make_shared<CKMS>(CKMS())),
    cur_window_(std::make_shared<CKMS>(CKMS())),
    last_asserted_time_(),
    cur_window_begin_(),
    window_size_(window_size) {
}

CKMSSample::Impl::~Impl() {
}

void CKMSSample::Impl::Clear() {
    std::lock_guard<std::mutex> lock{mutex_};
    prev_window_->reset();
    cur_window_->reset();
    last_asserted_time_ = std::chrono::time_point<Clock>();
    cur_window_begin_ = std::chrono::time_point<Clock>();
}

std::uint64_t CKMSSample::Impl::size(Clock::time_point timestamp) {
    return MakeSnapshot(timestamp).size();
}

std::uint64_t CKMSSample::Impl::size() {
    return size(Clock::now());
}

std::uint64_t CKMSSample::Impl::min() {
    return MakeSnapshot().min();
}

std::uint64_t CKMSSample::Impl::max() {
    return MakeSnapshot().max();
}

std::uint64_t CKMSSample::Impl::sum() {
    return MakeSnapshot().sum();
}

std::uint64_t CKMSSample::Impl::variance() {
    return MakeSnapshot().variance();
}

void CKMSSample::Impl::Update(std::int64_t value) {
  Update(value, Clock::now());
}

void CKMSSample::Impl::Update(std::int64_t value, Clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock{mutex_};
    AssertValidTime(timestamp);
    if (!IsInCurrentWindow(timestamp)) {
        // Enough time has passed, and the current window is no longer current.
        // We need to shift it.

        if (IsInNextWindow(timestamp)) {
            // The current window becomes the previous one.
            prev_window_.swap(cur_window_);
            cur_window_->reset();
            cur_window_begin_ += window_size_;
        } else {
            // We haven't had any input for long enough that both prev_window_ and cur_window_ should be empty.
            prev_window_->reset();
            cur_window_->reset();
            cur_window_begin_ = CalculateCurrentWindowStartingPoint(timestamp);
        }
    }
    cur_window_->insert(value);
}

Snapshot CKMSSample::Impl::MakeSnapshot(Clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock{mutex_};
    AssertValidTime(timestamp);
    if (IsInCurrentWindow(timestamp)) {
        return {*prev_window_};
    } else if (IsInNextWindow(timestamp)) {
        return {*cur_window_};
    } else {
        return {CKMS()};
    }
}

Snapshot CKMSSample::Impl::MakeSnapshot() {
    return MakeSnapshot(Clock::now());
}

} // namespace stats
} // namespace medida
