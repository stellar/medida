// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#ifndef MEDIDA_CKMS_SAMPLE_H_
#define MEDIDA_CKMS_SAMPLE_H_

#include <cstdint>
#include <memory>

#include "medida/types.h"
#include "medida/stats/sample.h"
#include "medida/stats/snapshot.h"

namespace medida {
namespace stats {

// CKMSSample maintains two N-second windows: one for the current window, and
// another for the previous window. It adds new data to the current one, and
// it reports the previous one.
//
// For instance, if N = 30 and it's 1:00:45,
// - it adds new data points to the current window [1:00:30, 1:01:00], and
// - it reports the previous window [1:00:00, 1:00:30].


// Each of size, Update, and MakeSnapshot has two versions, and
// the one without a timestamp calls the other one with the current time.
//
// Unless there's a good reason to do so,
// you should always use the one WITHOUT the timestamp.
//
// The one with a timestamp is generally used for testing. We pass a timestamp
// as a way to fast-forward time to make testing easier.
//
// Regardless of which ones you use, the only rule that the caller must follow is
// that you can't go back in time. After you use a timestamp T, you are not allowed
// to call another method with a timestamp S if S < T.
//
// Note: While it should not matter in practice, the window is technically defined
// to be a half-open interval [beginning, end) for testing purposes instead of a closed
// interval.

class CKMSSample : public Sample {
 public:
  CKMSSample(std::chrono::seconds window_size = std::chrono::seconds(30));
  ~CKMSSample();
  virtual void Clear();
  virtual std::uint64_t size() const;
  virtual std::uint64_t size(SystemClock::time_point timestamp) const;
  virtual void Update(std::int64_t value);
  virtual void Update(std::int64_t value, SystemClock::time_point timestamp);
  virtual Snapshot MakeSnapshot(uint64_t divisor = 1) const;
  virtual Snapshot MakeSnapshot(SystemClock::time_point timestamp, uint64_t divisor = 1) const;
 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace stats
} // namespace medida

#endif // MEDIDA_CKMS_SAMPLE_H_
