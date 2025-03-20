//
// Copyright (c) 2012 Daniel Lundin
//

#include "medida/counter.h"
#include <Tracy.hpp>

#include <atomic>

namespace medida {

class Counter::Impl {
 public:
  Impl(std::int64_t init = 0);
  ~Impl();
  void Process(MetricProcessor& processor);
  std::int64_t count() const;
  void set_count(std::int64_t n);
  void inc(std::int64_t n = 1);
  void dec(std::int64_t n = 1);
  void clear();
 private:
  std::atomic<std::int64_t> count_;
};


Counter::Counter(std::int64_t init)
    : impl_ {new Counter::Impl {init}} {
    ZoneScoped;
}


Counter::~Counter() {
    ZoneScoped;
}


void Counter::Process(MetricProcessor& processor)  {
    ZoneScoped;
    processor.Process(*this); // FIXME: pimpl?
}


std::int64_t Counter::count() const {
    ZoneScoped;
    return impl_->count();
}


void Counter::set_count(std::int64_t n) {
    ZoneScoped;
    return impl_->set_count(n);
}


void Counter::inc(std::int64_t n) {
    ZoneScoped;
    impl_->inc(n);
}


void Counter::dec(std::int64_t n) {
    ZoneScoped;
    impl_->dec(n);
}


void Counter::clear() {
    ZoneScoped;
    impl_->clear();
}


// === Implementation ===


Counter::Impl::Impl(std::int64_t init) : count_ {init} {
}


Counter::Impl::~Impl() {
}


std::int64_t Counter::Impl::count() const {
  return count_.load();
}


void Counter::Impl::set_count(std::int64_t n) {
  count_ = n;
}


void Counter::Impl::inc(std::int64_t n) {
  count_ += n;
}


void Counter::Impl::dec(std::int64_t n) {
  count_ -= n;
}


void Counter::Impl::clear() {
  set_count(0);
}


} // namespace medida