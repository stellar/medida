// Copyright 2026 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "medida/stats/log_histogram_sample.h"

#include <atomic>
#include <bit>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <thread>
#include <type_traits>

#ifdef __linux__
#include <ctime>
#endif

namespace medida {
namespace stats {

namespace {

static std::int64_t const kInvalidWindow = std::numeric_limits<std::int64_t>::min();
static std::int64_t const kPreparingWindow = kInvalidWindow + 1;

std::size_t SubBucketBits(std::size_t buckets_per_level) {
  if (!std::has_single_bit(buckets_per_level)) {
    throw std::invalid_argument("buckets_per_level must be a power of two");
  }
  return static_cast<std::size_t>(std::bit_width(buckets_per_level) - 1);
}

std::uint64_t Magnitude(std::int64_t value) {
  if (value >= 0) {
    return static_cast<std::uint64_t>(value);
  }
  if (value == std::numeric_limits<std::int64_t>::min()) {
    return std::uint64_t{1} << 63;
  }
  return static_cast<std::uint64_t>(-value);
}

std::size_t FloorLog2(std::uint64_t value) {
  return static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::digits -
                                  1 - std::countl_zero(value));
}

std::size_t LevelsForMaxAbsoluteValue(std::uint64_t max_absolute_value) {
  if (max_absolute_value == 0) {
    throw std::invalid_argument("max_absolute_value must be positive");
  }
  return FloorLog2(max_absolute_value) + 1;
}

SystemClock::time_point CoarseNow() {
#ifdef __linux__
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME_COARSE, &ts);
  return SystemClock::time_point(std::chrono::seconds(ts.tv_sec) +
                                 std::chrono::nanoseconds(ts.tv_nsec));
#else
  return SystemClock::now();
#endif
}

} // namespace

class LogHistogramSample::Impl {
 public:
  virtual ~Impl() {}
  virtual void Clear() = 0;
  virtual std::uint64_t size() const = 0;
  virtual std::uint64_t size(SystemClock::time_point timestamp) const = 0;
  virtual void Update(std::int64_t value) = 0;
  virtual void Update(std::int64_t value, SystemClock::time_point timestamp) = 0;
  virtual void UpdateMany(const std::vector<std::int64_t>& values,
                          SystemClock::time_point timestamp) = 0;
  virtual Snapshot MakeSnapshot(uint64_t divisor = 1) const = 0;
  virtual Snapshot MakeSnapshot(SystemClock::time_point timestamp,
                                uint64_t divisor = 1) const = 0;
};

template <typename Counter>
class LogHistogramSampleImpl : public LogHistogramSample::Impl {
  static_assert(std::is_unsigned<Counter>::value, "Counter must be unsigned");

 public:
  explicit LogHistogramSampleImpl(const LogHistogramSample::Config& config);
  ~LogHistogramSampleImpl() override;
  void Clear() override;
  std::uint64_t size() const override;
  std::uint64_t size(SystemClock::time_point timestamp) const override;
  void Update(std::int64_t value) override;
  void Update(std::int64_t value, SystemClock::time_point timestamp) override;
  void UpdateMany(const std::vector<std::int64_t>& values,
                  SystemClock::time_point timestamp) override;
  Snapshot MakeSnapshot(uint64_t divisor = 1) const override;
  Snapshot MakeSnapshot(SystemClock::time_point timestamp,
                        uint64_t divisor = 1) const override;

 private:
  class Window {
   public:
    Window();
    void Init(std::size_t bucket_count);
    void ClearTo(std::int64_t window_start);
    void Record(std::size_t bucket, std::int64_t value);
    std::uint64_t Size() const;
    std::int64_t Max() const;
    void CopyCounts(std::vector<std::uint64_t>& out) const;
    std::atomic<std::int64_t> window_start_;

   private:
    std::vector<std::atomic<Counter>> buckets_;
    std::atomic<std::uint64_t> size_;
    std::atomic<std::int64_t> max_;
  };

  std::int64_t WindowStart(SystemClock::time_point timestamp) const;
  std::size_t WindowIndex(std::int64_t window_start) const;
  void PrepareWindow(Window& window, std::int64_t window_start) const;
  std::size_t Bucket(std::int64_t value) const;
  std::size_t MagnitudeBucket(std::uint64_t magnitude) const;

  std::chrono::seconds const window_size_;
  std::int64_t const window_size_seconds_;
  std::size_t const buckets_per_level_;
  std::size_t const sub_bucket_bits_;
  std::uint64_t const max_absolute_value_;
  std::size_t const magnitude_buckets_;
  std::size_t const bucket_count_;
  Window windows_[2];
};

LogHistogramSample::LogHistogramSample(std::chrono::seconds window_size,
                                       std::size_t buckets_per_level)
    : LogHistogramSample(Config{window_size, buckets_per_level}) {
}

LogHistogramSample::LogHistogramSample(const Config& config) {
  if (config.max_events_per_window <= std::numeric_limits<std::uint16_t>::max()) {
    impl_ = std::unique_ptr<Impl>(new LogHistogramSampleImpl<std::uint16_t>(config));
  } else if (config.max_events_per_window <=
             std::numeric_limits<std::uint32_t>::max()) {
    impl_ = std::unique_ptr<Impl>(new LogHistogramSampleImpl<std::uint32_t>(config));
  } else {
    impl_ = std::unique_ptr<Impl>(new LogHistogramSampleImpl<std::uint64_t>(config));
  }
}

LogHistogramSample::~LogHistogramSample() {
}

void LogHistogramSample::Clear() {
  impl_->Clear();
}

std::uint64_t LogHistogramSample::size() const {
  return impl_->size();
}

std::uint64_t LogHistogramSample::size(SystemClock::time_point timestamp) const {
  return impl_->size(timestamp);
}

void LogHistogramSample::Update(std::int64_t value) {
  impl_->Update(value);
}

void LogHistogramSample::Update(std::int64_t value,
                                SystemClock::time_point timestamp) {
  impl_->Update(value, timestamp);
}

void LogHistogramSample::UpdateMany(const std::vector<std::int64_t>& values) {
  impl_->UpdateMany(values, CoarseNow());
}

void LogHistogramSample::UpdateMany(const std::vector<std::int64_t>& values,
                                    SystemClock::time_point timestamp) {
  impl_->UpdateMany(values, timestamp);
}

Snapshot LogHistogramSample::MakeSnapshot(uint64_t divisor) const {
  return impl_->MakeSnapshot(divisor);
}

Snapshot LogHistogramSample::MakeSnapshot(SystemClock::time_point timestamp,
                                          uint64_t divisor) const {
  return impl_->MakeSnapshot(timestamp, divisor);
}

template <typename Counter>
LogHistogramSampleImpl<Counter>::Window::Window()
    : window_start_(kInvalidWindow), buckets_(), size_(0),
      max_(std::numeric_limits<std::int64_t>::min()) {
}

template <typename Counter>
void LogHistogramSampleImpl<Counter>::Window::Init(std::size_t bucket_count) {
  std::vector<std::atomic<Counter>> buckets(bucket_count);
  buckets_.swap(buckets);
  ClearTo(kInvalidWindow);
}

template <typename Counter>
void LogHistogramSampleImpl<Counter>::Window::ClearTo(std::int64_t window_start) {
  window_start_.store(kPreparingWindow, std::memory_order_release);
  for (auto& bucket : buckets_) {
    bucket.store(0, std::memory_order_relaxed);
  }
  size_.store(0, std::memory_order_relaxed);
  max_.store(std::numeric_limits<std::int64_t>::min(), std::memory_order_relaxed);
  window_start_.store(window_start, std::memory_order_release);
}

template <typename Counter>
void LogHistogramSampleImpl<Counter>::Window::Record(std::size_t bucket,
                                                     std::int64_t value) {
  buckets_[bucket].fetch_add(1, std::memory_order_relaxed);
  size_.fetch_add(1, std::memory_order_relaxed);

  auto current = max_.load(std::memory_order_relaxed);
  while (value > current &&
         !max_.compare_exchange_weak(current, value,
                                     std::memory_order_relaxed,
                                     std::memory_order_relaxed)) {
  }
}

template <typename Counter>
std::uint64_t LogHistogramSampleImpl<Counter>::Window::Size() const {
  return size_.load(std::memory_order_relaxed);
}

template <typename Counter>
std::int64_t LogHistogramSampleImpl<Counter>::Window::Max() const {
  return max_.load(std::memory_order_relaxed);
}

template <typename Counter>
void LogHistogramSampleImpl<Counter>::Window::CopyCounts(
    std::vector<std::uint64_t>& out) const {
  out.resize(buckets_.size());
  for (std::size_t i = 0; i < buckets_.size(); ++i) {
    out[i] = buckets_[i].load(std::memory_order_relaxed);
  }
}

template <typename Counter>
LogHistogramSampleImpl<Counter>::LogHistogramSampleImpl(
    const LogHistogramSample::Config& config)
    : window_size_(config.window_size),
      window_size_seconds_(config.window_size.count()),
      buckets_per_level_(config.buckets_per_level),
      sub_bucket_bits_(SubBucketBits(config.buckets_per_level)),
      max_absolute_value_(config.max_absolute_value),
      magnitude_buckets_(LevelsForMaxAbsoluteValue(config.max_absolute_value) *
                         config.buckets_per_level),
      bucket_count_(2 * magnitude_buckets_ + 1),
      windows_() {
  if (window_size_seconds_ <= 0) {
    throw std::invalid_argument("window_size must be positive");
  }
  windows_[0].Init(bucket_count_);
  windows_[1].Init(bucket_count_);
}

template <typename Counter>
LogHistogramSampleImpl<Counter>::~LogHistogramSampleImpl() {
}

template <typename Counter>
void LogHistogramSampleImpl<Counter>::Clear() {
  windows_[0].ClearTo(kInvalidWindow);
  windows_[1].ClearTo(kInvalidWindow);
}

template <typename Counter>
std::int64_t LogHistogramSampleImpl<Counter>::WindowStart(
    SystemClock::time_point timestamp) const {
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
      timestamp.time_since_epoch()).count();
  auto offset = seconds % window_size_seconds_;
  if (offset < 0) {
    offset += window_size_seconds_;
  }
  return seconds - offset;
}

template <typename Counter>
std::size_t LogHistogramSampleImpl<Counter>::WindowIndex(
    std::int64_t window_start) const {
  auto sequence = window_start / window_size_seconds_;
  return static_cast<std::size_t>(sequence & 1);
}

template <typename Counter>
void LogHistogramSampleImpl<Counter>::PrepareWindow(
    Window& window, std::int64_t window_start) const {
  for (;;) {
    auto current = window.window_start_.load(std::memory_order_acquire);
    if (current == window_start) {
      return;
    }
    if (current == kPreparingWindow) {
      std::this_thread::yield();
      continue;
    }
    if (window.window_start_.compare_exchange_strong(
            current, kPreparingWindow, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      window.ClearTo(window_start);
      return;
    }
  }
}

template <typename Counter>
std::size_t LogHistogramSampleImpl<Counter>::MagnitudeBucket(
    std::uint64_t magnitude) const {
  magnitude = std::min(magnitude, max_absolute_value_);
  auto level = FloorLog2(magnitude);
  auto base = std::uint64_t{1} << level;
  auto remainder = magnitude - base;
  std::uint64_t offset;
  if (level >= sub_bucket_bits_) {
    offset = remainder >> (level - sub_bucket_bits_);
  } else {
    offset = remainder << (sub_bucket_bits_ - level);
  }
  return level * buckets_per_level_ + offset;
}

template <typename Counter>
std::size_t LogHistogramSampleImpl<Counter>::Bucket(std::int64_t value) const {
  if (value == 0) {
    return magnitude_buckets_;
  }

  auto magnitude_bucket = MagnitudeBucket(Magnitude(value));
  if (value < 0) {
    return magnitude_buckets_ - 1 - magnitude_bucket;
  }
  return magnitude_buckets_ + 1 + magnitude_bucket;
}

template <typename Counter>
void LogHistogramSampleImpl<Counter>::Update(std::int64_t value) {
  Update(value, CoarseNow());
}

template <typename Counter>
void LogHistogramSampleImpl<Counter>::Update(std::int64_t value,
                                             SystemClock::time_point timestamp) {
  auto window_start = WindowStart(timestamp);
  auto& window = windows_[WindowIndex(window_start)];
  PrepareWindow(window, window_start);
  window.Record(Bucket(value), value);
}

template <typename Counter>
void LogHistogramSampleImpl<Counter>::UpdateMany(
    const std::vector<std::int64_t>& values,
    SystemClock::time_point timestamp) {
  auto window_start = WindowStart(timestamp);
  auto& window = windows_[WindowIndex(window_start)];
  PrepareWindow(window, window_start);
  for (auto value : values) {
    window.Record(Bucket(value), value);
  }
}

template <typename Counter>
std::uint64_t LogHistogramSampleImpl<Counter>::size(
    SystemClock::time_point timestamp) const {
  auto previous_start = WindowStart(timestamp) - window_size_seconds_;
  auto const& window = windows_[WindowIndex(previous_start)];
  if (window.window_start_.load(std::memory_order_acquire) != previous_start) {
    return 0;
  }
  return window.Size();
}

template <typename Counter>
std::uint64_t LogHistogramSampleImpl<Counter>::size() const {
  return size(CoarseNow());
}

template <typename Counter>
Snapshot LogHistogramSampleImpl<Counter>::MakeSnapshot(
    SystemClock::time_point timestamp, uint64_t divisor) const {
  auto previous_start = WindowStart(timestamp) - window_size_seconds_;
  auto const& window = windows_[WindowIndex(previous_start)];
  if (window.window_start_.load(std::memory_order_acquire) != previous_start) {
    return {std::vector<std::uint64_t>(bucket_count_), buckets_per_level_, 0, divisor};
  }

  std::vector<std::uint64_t> counts;
  window.CopyCounts(counts);
  auto max = window.Max();
  if (window.window_start_.load(std::memory_order_acquire) != previous_start) {
    return {std::vector<std::uint64_t>(bucket_count_), buckets_per_level_, 0, divisor};
  }
  return {counts, buckets_per_level_, max, divisor};
}

template <typename Counter>
Snapshot LogHistogramSampleImpl<Counter>::MakeSnapshot(uint64_t divisor) const {
  return MakeSnapshot(CoarseNow(), divisor);
}

} // namespace stats
} // namespace medida