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

namespace medida {
namespace stats {

class CKMSSample::Impl {
 public:
  Impl(std::chrono::seconds = std::chrono::seconds(30));
  ~Impl();
  void Clear();
  std::uint64_t size();
  std::uint64_t size(SystemClock::time_point timestamp);
  void Update(std::int64_t value);
  void Update(std::int64_t value, SystemClock::time_point timestamp);
  Snapshot MakeSnapshot(uint64_t divisor = 1);
  Snapshot MakeSnapshot(SystemClock::time_point timestamp, uint64_t divisor = 1);
 private:
  std::mutex mutex_;
  std::shared_ptr<CKMS> prev_window_, cur_window_;
  SystemClock::time_point cur_window_begin_;
  std::chrono::seconds window_size_;
  SystemClock::time_point CalculateCurrentWindowStartingPoint(SystemClock::time_point time) const;
  bool IsInCurrentWindow(SystemClock::time_point const& timestamp) const;
  bool IsInPreviousWindow(SystemClock::time_point const& timestamp) const;
  bool IsInNextWindow(SystemClock::time_point const& timestamp) const;
  bool AdvanceWindows(SystemClock::time_point timestamp);
};

CKMSSample::CKMSSample(std::chrono::seconds window_size) : impl_ {new CKMSSample::Impl {window_size}} {
}


CKMSSample::~CKMSSample() {
}

void CKMSSample::Clear() {
  impl_->Clear();
}

std::uint64_t CKMSSample::size() const {
  return impl_->size();
}

std::uint64_t CKMSSample::size(SystemClock::time_point timestamp) const {
  return impl_->size(timestamp);
}

void CKMSSample::Update(std::int64_t value) {
  impl_->Update(value);
}

void CKMSSample::Update(std::int64_t value, SystemClock::time_point timestamp) {
  impl_->Update(value, timestamp);
}

Snapshot CKMSSample::MakeSnapshot(uint64_t divisor) const {
  return impl_->MakeSnapshot(divisor);
}

Snapshot CKMSSample::MakeSnapshot(SystemClock::time_point timestamp, uint64_t divisor) const {
  return impl_->MakeSnapshot(timestamp, divisor);
}

// === Implementation ===

SystemClock::time_point CKMSSample::Impl::CalculateCurrentWindowStartingPoint(SystemClock::time_point time) const {
    return time - (std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()) % window_size_);
}

bool CKMSSample::Impl::IsInCurrentWindow(SystemClock::time_point const& timestamp) const {
    return cur_window_begin_ <= timestamp && timestamp < cur_window_begin_ + window_size_;
}

bool CKMSSample::Impl::IsInPreviousWindow(SystemClock::time_point const& timestamp) const {
    return timestamp < cur_window_begin_ && timestamp + window_size_ >= cur_window_begin_;
}

bool CKMSSample::Impl::IsInNextWindow(SystemClock::time_point const& timestamp) const {
    return cur_window_begin_ + window_size_ <= timestamp && timestamp < cur_window_begin_ + 2 * window_size_;
}

bool CKMSSample::Impl::AdvanceWindows(SystemClock::time_point timestamp) {
    if (!IsInCurrentWindow(timestamp)) {
        if (IsInPreviousWindow(timestamp))
        {
            // A minor backward system clock adjustment occured
            // or a race occured recording samples (see mutex above)
            // in either case drop events as we want to keep the previous window
            // immutable
            return false;
        }

        if (IsInNextWindow(timestamp)) {
            // Enough time has passed, and the current window is no longer current.
            // We need to shift it.
            // The current window becomes the previous one.
            prev_window_.swap(cur_window_);
            cur_window_->reset();
            cur_window_begin_ += window_size_;
        } else {
            // Either we haven't had any input for a long time
            // or the system clock moved backwards by a lot
            // in either case prev_window_ and cur_window_ should be empty.
            prev_window_->reset();
            cur_window_->reset();
            cur_window_begin_ = CalculateCurrentWindowStartingPoint(timestamp);
        }
    }
    return true;
}

CKMSSample::Impl::Impl(std::chrono::seconds window_size) :
    prev_window_(std::make_shared<CKMS>(CKMS())),
    cur_window_(std::make_shared<CKMS>(CKMS())),
    cur_window_begin_(),
    window_size_(window_size) {
}

CKMSSample::Impl::~Impl() {
}

void CKMSSample::Impl::Clear() {
    std::lock_guard<std::mutex> lock{mutex_};
    prev_window_->reset();
    cur_window_->reset();
    cur_window_begin_ = std::chrono::time_point<SystemClock>();
}

std::uint64_t CKMSSample::Impl::size(SystemClock::time_point timestamp) {
    return MakeSnapshot(timestamp).size();
}

std::uint64_t CKMSSample::Impl::size() {
    return size(SystemClock::now());
}

void CKMSSample::Impl::Update(std::int64_t value) {
  Update(value, SystemClock::now());
}

void CKMSSample::Impl::Update(std::int64_t value, SystemClock::time_point timestamp) {
    std::lock_guard<std::mutex> lock{mutex_};
    if (AdvanceWindows(timestamp)) {
        cur_window_->insert(value);
    }
}

Snapshot CKMSSample::Impl::MakeSnapshot(SystemClock::time_point timestamp, uint64_t divisor) {
    std::lock_guard<std::mutex> lock{mutex_};
    if (AdvanceWindows(timestamp)) {
        return {*prev_window_, divisor};
    } else {
        return {CKMS()};
    }
}

Snapshot CKMSSample::Impl::MakeSnapshot(uint64_t divisor) {
    return MakeSnapshot(SystemClock::now(), divisor);
}

} // namespace stats
} // namespace medida
