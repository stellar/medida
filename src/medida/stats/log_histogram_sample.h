// Copyright 2026 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#ifndef MEDIDA_LOG_HISTOGRAM_SAMPLE_H_
#define MEDIDA_LOG_HISTOGRAM_SAMPLE_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "medida/types.h"
#include "medida/stats/sample.h"
#include "medida/stats/snapshot.h"

namespace medida {
namespace stats {

// LogHistogramSample is a low-overhead alternative to CKMSSample. It keeps two
// fixed time windows and records into logarithmic buckets with relaxed atomic
// increments. Updates allocate no memory and take no locks.
//
// buckets_per_level must be a power of two. The default of 8 gives up to 12.5%
// bucket-width granularity per power-of-two value range, and about half that
// when reporting bucket midpoints.
class LogHistogramSample : public Sample {
 public:
  static const std::size_t kDefaultBucketsPerLevel = 8;
  static const std::uint64_t kDefaultMaxAbsoluteValue =
      (std::uint64_t{1} << 48) - 1;
  static const std::uint64_t kDefaultMaxEventsPerWindow =
      std::numeric_limits<std::uint32_t>::max();

  struct Config {
    std::chrono::seconds window_size = std::chrono::seconds(30);
    std::size_t buckets_per_level = kDefaultBucketsPerLevel;
    std::uint64_t max_absolute_value = kDefaultMaxAbsoluteValue;
    std::uint64_t max_events_per_window = kDefaultMaxEventsPerWindow;
  };

  LogHistogramSample(
      std::chrono::seconds window_size = std::chrono::seconds(30),
      std::size_t buckets_per_level = kDefaultBucketsPerLevel);
  explicit LogHistogramSample(const Config& config);
  ~LogHistogramSample();

  virtual void Clear() override;
  virtual std::uint64_t size() const override;
  virtual std::uint64_t size(SystemClock::time_point timestamp) const;
  virtual void Update(std::int64_t value) override;
  virtual void Update(std::int64_t value, SystemClock::time_point timestamp);
  virtual void UpdateMany(const std::vector<std::int64_t>& values) override;
  virtual void UpdateMany(const std::vector<std::int64_t>& values,
                          SystemClock::time_point timestamp);
  virtual Snapshot MakeSnapshot(uint64_t divisor = 1) const override;
  virtual Snapshot MakeSnapshot(SystemClock::time_point timestamp,
                                uint64_t divisor = 1) const;
  class Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

} // namespace stats
} // namespace medida

#endif // MEDIDA_LOG_HISTOGRAM_SAMPLE_H_