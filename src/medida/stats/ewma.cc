//
// Copyright (c) 2012 Daniel Lundin
//

#include "medida/stats/ewma.h"

#include <atomic>
#include <cmath>

namespace medida {
namespace stats {

static const int kINTERVAL = 5;
static const double kSECONDS_PER_MINUTE = 60.0;
static const int kONE_MINUTE = 1;
static const int kFIVE_MINUTES = 5;
static const int kFIFTEEN_MINUTES = 15;
static const double kM1_ALPHA = 1 - std::exp(-kINTERVAL / kSECONDS_PER_MINUTE / kONE_MINUTE);
static const double kM5_ALPHA = 1 - std::exp(-kINTERVAL / kSECONDS_PER_MINUTE / kFIVE_MINUTES);
static const double kM15_ALPHA = 1 - std::exp(-kINTERVAL / kSECONDS_PER_MINUTE / kFIFTEEN_MINUTES);

class EWMA::Impl {
 public:
  Impl(double alpha, std::chrono::nanoseconds interval);
  Impl(Impl &other);
  ~Impl();
  void update(std::int64_t n);
  void tick();
  double getRate(std::chrono::nanoseconds duration = std::chrono::seconds {1}) const;
  void clear();
 private:
  std::atomic<bool> initialized_;
  std::atomic<double> rate_;
  std::atomic<std::int64_t> uncounted_;
  const double alpha_;
  const std::int64_t interval_nanos_;
};


EWMA::EWMA(double alpha, std::chrono::nanoseconds interval)
    : impl_ {new EWMA::Impl {alpha, interval}} {
}


EWMA::EWMA(EWMA &&other) 
    : impl_ {new EWMA::Impl{*other.impl_}} {
}


EWMA::~EWMA() {
}


EWMA EWMA::oneMinuteEWMA() {
  return {kM1_ALPHA, std::chrono::seconds{5}};
}


EWMA EWMA::fiveMinuteEWMA() {
  return {kM5_ALPHA, std::chrono::seconds{5}};
}


EWMA EWMA::fifteenMinuteEWMA() {
  return {kM15_ALPHA, std::chrono::seconds{5}};
}


void EWMA::update(std::int64_t n) {
  impl_->update(n);
}


void EWMA::tick() {
  impl_->tick();
}


double EWMA::getRate(std::chrono::nanoseconds duration) const {
  return impl_->getRate(duration);
}

void EWMA::clear()
{
  impl_->clear();
}

// === Implementation ===


EWMA::Impl::Impl(double alpha, std::chrono::nanoseconds interval)
    : initialized_    {false},
      rate_           {0.0},
      uncounted_      {0},
      alpha_          {alpha},
      interval_nanos_ {interval.count()} {
}


EWMA::Impl::Impl(Impl &other)
    : initialized_    {other.initialized_.load(std::memory_order_relaxed)},
      rate_           {other.rate_.load(std::memory_order_relaxed)},
      uncounted_      {other.uncounted_.load(std::memory_order_relaxed)},
      alpha_          {other.alpha_},
      interval_nanos_ {other.interval_nanos_} {
}


EWMA::Impl::~Impl() {
}


void EWMA::Impl::update(std::int64_t n) {
  uncounted_.fetch_add(n, std::memory_order_relaxed);
}


void EWMA::Impl::tick() {
  double count = uncounted_.exchange(0, std::memory_order_relaxed);
  auto instantRate = count / interval_nanos_;
  if (initialized_.load(std::memory_order_relaxed)) {
    auto rate = rate_.load(std::memory_order_relaxed);
    rate_.store(rate + (alpha_ * (instantRate - rate)), std::memory_order_relaxed);
  } else {
    rate_.store(instantRate, std::memory_order_relaxed);
    initialized_.store(true, std::memory_order_relaxed);
  }
}


double EWMA::Impl::getRate(std::chrono::nanoseconds duration) const {
  return rate_.load(std::memory_order_relaxed) * duration.count();
}

void EWMA::Impl::clear()
{
  initialized_.store(false, std::memory_order_relaxed);
  rate_.store(0.0, std::memory_order_relaxed);
  uncounted_.store(0, std::memory_order_relaxed);
}

} // namespace stats
} // namespace medida
