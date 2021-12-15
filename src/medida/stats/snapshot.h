//
// Copyright (c) 2012 Daniel Lundin
//

#ifndef MEDIDA_METRICS_SNAPSHOT_H_
#define MEDIDA_METRICS_SNAPSHOT_H_

#include "medida/stats/ckms.h"

#include <memory>
#include <vector>

namespace medida {
namespace stats {

class Snapshot {
 public:
  Snapshot(const std::vector<double>& values, uint64_t divisor = 1);
  Snapshot(const CKMS& ckms, uint64_t divisor = 1);
  ~Snapshot();
  Snapshot(Snapshot const&) = delete;
  Snapshot& operator=(Snapshot const&) = delete;
  Snapshot(Snapshot&&);
  std::size_t size() const;
  double getValue(double quantile);
  double getMedian();
  double get75thPercentile();
  double get95thPercentile();
  double get98thPercentile();
  double get99thPercentile();
  double get999thPercentile();
  double max();
  std::vector<double> getValues() const;
  class Impl;
  class VectorImpl;
  class CKMSImpl;
 private:
  void checkImpl() const;
  std::unique_ptr<Impl> impl_;
};


} // namespace stats
} // namespace medida

#endif // MEDIDA_METRICS_SNAPSHOT_H_
