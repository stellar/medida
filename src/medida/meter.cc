//
// Copyright (c) 2012 Daniel Lundin
//

#include "medida/meter.h"
#include "medida/tracy.h"

#include <atomic>
#include <thread>

namespace medida {

static const auto kTickInterval = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(5)).count();

class Meter::Impl {
 public:
  Impl(std::string event_type, std::chrono::nanoseconds rate_unit = std::chrono::seconds(1));
  ~Impl();
  std::chrono::nanoseconds rate_unit() const;
  std::string event_type() const;
  std::uint64_t count() const;
  double fifteen_minute_rate();
  double five_minute_rate();
  double one_minute_rate();
  double mean_rate();
  void Mark(std::uint64_t n = 1);
  void Clear();
  void Process(MetricProcessor& processor);
 private:
  const std::string event_type_;
  const std::chrono::nanoseconds rate_unit_;
  std::atomic<std::uint64_t> count_;
  std::atomic<std::int64_t> start_time_;
  std::atomic<std::int64_t> last_tick_;
  stats::EWMA m1_rate_;
  stats::EWMA m5_rate_;
  stats::EWMA m15_rate_;
  std::atomic<bool> ticking_;
  static std::int64_t Now();
  void Tick();
  void TickIfNecessary();
};


Meter::Meter(std::string event_type, std::chrono::nanoseconds rate_unit)
    : impl_ {new Meter::Impl {event_type, rate_unit}} {
    ZoneScoped;
}


Meter::~Meter() {
    ZoneScoped;
}


std::chrono::nanoseconds Meter::rate_unit() const {
    ZoneScoped;
    return impl_->rate_unit();
}


std::string Meter::event_type() const {
    ZoneScoped;
    return impl_->event_type();
}


std::uint64_t Meter::count() const {
    ZoneScoped;
    return impl_->count();
}


double Meter::fifteen_minute_rate() {
    ZoneScoped;
    return impl_->fifteen_minute_rate();
}


double Meter::five_minute_rate() {
    ZoneScoped;
    return impl_->five_minute_rate();
}


double Meter::one_minute_rate() {
    ZoneScoped;
    return impl_->one_minute_rate();
}


double Meter::mean_rate() {
    ZoneScoped;
    return impl_->mean_rate();
}


void Meter::Mark(std::uint64_t n) {
    ZoneScoped;
    impl_->Mark(n);
}

void Meter::Clear()
{
    ZoneScoped;
    impl_->Clear();
}

void Meter::Process(MetricProcessor& processor) {
    ZoneScoped;
    processor.Process(*this); // FIXME: pimpl?
}


// === Implementation ===


Meter::Impl::Impl(std::string event_type, std::chrono::nanoseconds rate_unit) 
    : event_type_ (event_type),
      rate_unit_  (rate_unit),
      count_      (0),
      start_time_ (Now()),
      last_tick_  (start_time_.load(std::memory_order_relaxed)),
      m1_rate_    (stats::EWMA::oneMinuteEWMA()),
      m5_rate_    (stats::EWMA::fiveMinuteEWMA()),
      m15_rate_   (stats::EWMA::fifteenMinuteEWMA()),
      ticking_    (false) {
}


Meter::Impl::~Impl() {
}


std::int64_t Meter::Impl::Now() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}


std::chrono::nanoseconds Meter::Impl::rate_unit() const {
  return rate_unit_;
}


std::string Meter::Impl::event_type() const {
  return event_type_;
}


std::uint64_t Meter::Impl::count() const {
  return count_.load(std::memory_order_relaxed);
}


double Meter::Impl::fifteen_minute_rate() {
  TickIfNecessary();
  return m15_rate_.getRate();
}


double Meter::Impl::five_minute_rate() {
  TickIfNecessary();
  return m5_rate_.getRate();
}


double Meter::Impl::one_minute_rate() {
  TickIfNecessary();
  return m1_rate_.getRate();
}


double Meter::Impl::mean_rate() {
  double c = count_.load(std::memory_order_relaxed);
  auto start_time = start_time_.load(std::memory_order_relaxed);
  auto elapsed = Now() - start_time;
  if (c > 0 && elapsed > 0) {
    return c * rate_unit_.count() / elapsed;
  }
  return 0.0;
}


void Meter::Impl::Mark(std::uint64_t n) {
  TickIfNecessary();
  count_.fetch_add(n, std::memory_order_relaxed);
  m1_rate_.update(n);
  m5_rate_.update(n);
  m15_rate_.update(n);
}

void Meter::Impl::Clear()
{
  bool expected = false;
  while (!ticking_.compare_exchange_weak(expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
    expected = false;
    std::this_thread::yield();
  }

  auto now = Now();
  count_.store(0, std::memory_order_relaxed);
  start_time_.store(now, std::memory_order_relaxed);
  last_tick_.store(now, std::memory_order_relaxed);
  m1_rate_.clear();
  m5_rate_.clear();
  m15_rate_.clear();
  ticking_.store(false, std::memory_order_release);
}

void Meter::Impl::Tick() {
  m1_rate_.tick();
  m5_rate_.tick();
  m15_rate_.tick();
}


void Meter::Impl::TickIfNecessary() {
  auto old_tick = last_tick_.load(std::memory_order_relaxed);
  auto new_tick = Now();
  auto age = new_tick - old_tick;
  if (age > kTickInterval) {
    bool expected = false;
    if (!ticking_.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
      return;
    }

    old_tick = last_tick_.load(std::memory_order_relaxed);
    new_tick = Now();
    age = new_tick - old_tick;
    if (age > kTickInterval) {
      auto required_ticks = age / kTickInterval;
      for (auto i = 0; i < required_ticks; i ++) {
        Tick();
      }
      last_tick_.store(new_tick, std::memory_order_release);
    }
    ticking_.store(false, std::memory_order_release);
  }
}


} // namespace medida
