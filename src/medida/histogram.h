//
// Copyright (c) 2012 Daniel Lundin
//

#ifndef MEDIDA_HISTOGRAM_H_
#define MEDIDA_HISTOGRAM_H_

#include <cstdint>
#include <memory>
#include <chrono>

#include "medida/metric_interface.h"
#include "medida/sampling_interface.h"
#include "medida/summarizable_interface.h"
#include "medida/stats/sample.h"

namespace medida {

class Histogram : public MetricInterface, SamplingInterface, SummarizableInterface {
 public:
  Histogram(SampleType sample_type = kCKMS,
            std::chrono::seconds ckms_window_size = std::chrono::seconds(30));
  ~Histogram();
  virtual stats::Snapshot GetSnapshot() const override;

  // The Histogram class introduces a second version of GetSnapshot
  // which takes divisor and asks that each sample be divided by it.
  // This is useful for the Timer class. For instance,
  // one might consider logging everything in nanoseconds,
  // and ask for metrics in microseconds in order to prevent
  // small samples from being ignored as rounding errors.
  virtual stats::Snapshot GetSnapshot(uint64_t divisor) const;
  virtual double sum() const override;
  virtual double max() const override;
  virtual double min() const override;
  virtual double mean() const override;
  virtual double std_dev() const override;
  void Update(std::int64_t value);
  std::uint64_t count() const;
  double variance() const;
  void Process(MetricProcessor& processor) override;
  void Clear();
 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace medida

#endif // MEDIDA_HISTOGRAM_H_
