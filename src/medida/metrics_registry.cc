//
// Copyright (c) 2012 Daniel Lundin
//

#include "medida/metrics_registry.h"

#include <algorithm>
#include <mutex>
#include <thread>

#include "medida/metric_name.h"

namespace medida {

class MetricsRegistry::Impl {
 public:
  Impl(std::chrono::seconds ckms_window_size = std::chrono::seconds(30));
  ~Impl();
  Counter& NewCounter(const MetricName &name, std::int64_t init_value = 0);
  Histogram& NewHistogram(const MetricName &name,
      SamplingInterface::SampleType sample_type = SamplingInterface::kCKMS);
  Meter& NewMeter(const MetricName &name, std::string event_type, 
      Clock::duration rate_unit = std::chrono::seconds(1));
  Timer& NewTimer(const MetricName &name,
      std::chrono::nanoseconds duration_unit = std::chrono::milliseconds(1),
      std::chrono::nanoseconds rate_unit = std::chrono::seconds(1));
  Buckets& NewBuckets(
      const MetricName& name, std::set<double> boundaries,
      std::chrono::nanoseconds duration_unit,
      std::chrono::nanoseconds rate_unit);

  std::map<MetricName, std::shared_ptr<MetricInterface>> GetAllMetrics() const;
  void ProcessAll(MetricProcessor& processor);
 private:
  std::map<MetricName, std::shared_ptr<MetricInterface>> metrics_;
  std::chrono::seconds const ckms_window_size_;
  mutable std::mutex mutex_;
  template<typename T, typename... Args> T& NewMetric(const MetricName& name, Args... args);
};


MetricsRegistry::MetricsRegistry(std::chrono::seconds ckms_window_size)
    : impl_ {new MetricsRegistry::Impl(ckms_window_size)} {
}


MetricsRegistry::~MetricsRegistry() {
}


Counter& MetricsRegistry::NewCounter(const MetricName &name, std::int64_t init_value) {
  return impl_->NewCounter(name, init_value);
}


Histogram& MetricsRegistry::NewHistogram(const MetricName &name,
    SamplingInterface::SampleType sample_type) {
  return impl_->NewHistogram(name, sample_type);
}


Meter& MetricsRegistry::NewMeter(const MetricName &name, std::string event_type,
    Clock::duration rate_unit) {
  return impl_->NewMeter(name, event_type, rate_unit);
}


Timer& MetricsRegistry::NewTimer(const MetricName &name, std::chrono::nanoseconds duration_unit,
    std::chrono::nanoseconds rate_unit) {
  return impl_->NewTimer(name, duration_unit, rate_unit);
}

Buckets&
MetricsRegistry::NewBuckets(const MetricName& name,
                                  std::set<double> boundaries,
                                  std::chrono::nanoseconds duration_unit,
                                  std::chrono::nanoseconds rate_unit)
{
    return impl_->NewBuckets(name, boundaries, duration_unit, rate_unit);
}

std::map<MetricName, std::shared_ptr<MetricInterface>> MetricsRegistry::GetAllMetrics() const {
  return impl_->GetAllMetrics();
}


// === Implementation ===


MetricsRegistry::Impl::Impl(std::chrono::seconds ckms_window_size)
    : ckms_window_size_(ckms_window_size) {
}


MetricsRegistry::Impl::~Impl() {
}


Counter& MetricsRegistry::Impl::NewCounter(const MetricName &name, std::int64_t init_value) {
  return NewMetric<Counter>(name, init_value);
}


Histogram& MetricsRegistry::Impl::NewHistogram(const MetricName &name,
    SamplingInterface::SampleType sample_type) {
  return NewMetric<Histogram>(name, sample_type, ckms_window_size_);
}


Meter& MetricsRegistry::Impl::NewMeter(const MetricName &name, std::string event_type,
    Clock::duration rate_unit) {
  return NewMetric<Meter>(name, event_type, rate_unit);
}


Timer& MetricsRegistry::Impl::NewTimer(const MetricName &name, std::chrono::nanoseconds duration_unit,
    std::chrono::nanoseconds rate_unit) {
  return NewMetric<Timer>(name, duration_unit, rate_unit, ckms_window_size_);
}

Buckets& MetricsRegistry::Impl::NewBuckets(
    const MetricName& name, std::set<double> boundaries,
    std::chrono::nanoseconds duration_unit,
    std::chrono::nanoseconds rate_unit)
{
    return NewMetric<Buckets>(name, boundaries, duration_unit, rate_unit);
}


template<typename MetricType, typename... Args>
MetricType& MetricsRegistry::Impl::NewMetric(const MetricName& name, Args... args) {
  std::lock_guard<std::mutex> lock {mutex_};
  if (metrics_.find(name) == std::end(metrics_)) {
    // GCC 4.6: Bug 44436 emplace* not implemented. Use ::reset instead.
    // metrics_[name].reset(new MetricType(args...));
    metrics_[name] = std::make_shared<MetricType>(args...);
  } else {
  }
  return dynamic_cast<MetricType&>(*metrics_[name]);
}

std::map<MetricName, std::shared_ptr<MetricInterface>> MetricsRegistry::Impl::GetAllMetrics() const {
 return {metrics_}; 
}


} // namespace medida