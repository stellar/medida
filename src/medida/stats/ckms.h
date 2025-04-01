// This file is originally from the Prometheus project, with
// local modifications made by Stellar Development Foundation.
//
// Copyright (c) 2016-2019 Jupp Mueller
// Copyright (c) 2017-2019 Gregor Jasny
//
// And many contributors, see
// https://github.com/jupp0r/prometheus-cpp/graphs/contributors
//
// Licensed under MIT license.
// https://opensource.org/licenses/MIT

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

namespace medida {
namespace stats {

class CKMS {
 public:
  CKMS();
  virtual ~CKMS() = default;

  // Make non-copyable and non-movable
  CKMS(const CKMS&) = delete;
  CKMS& operator=(const CKMS&) = delete;
  CKMS(CKMS&&) = delete;
  CKMS& operator=(CKMS&&) = delete;

  virtual void insert(double value) = 0;
  virtual double get(double q) = 0;
  virtual void reset() = 0;
  virtual std::size_t count() const = 0;
  virtual double max() const = 0;
};

// Define factory function type
using CKMSFactoryFunc = std::function<std::shared_ptr<CKMS>()>;

// Global factory function that can be changed by users
extern CKMSFactoryFunc createCKMS;

class CKMSImpl : public CKMS {
 public:
  struct Quantile {
    Quantile(double quantile, double error);

    double quantile;
    double error;
    double u;
    double v;
  };

 private:
  struct Item {
    double value;
    int g;
    int delta;

    Item(double value, int lower_delta, int delta);
  };

 public:
  CKMSImpl();
  explicit CKMSImpl(const std::vector<Quantile>& quantiles);

  void insert(double value) override;
  double get(double q) override;
  void reset() override;
  std::size_t count() const override;
  double max() const override;

 private:
  const std::reference_wrapper<const std::vector<Quantile>> quantiles_;
  double allowableError(int rank);
  bool insertBatch();
  void compress();

  std::size_t count_;
  std::vector<Item> sample_;
  std::array<double, 500> buffer_;
  std::size_t buffer_count_;
  std::size_t size_when_last_sorted_;
  double max_;
};

} // namespace stats
} // namespace medida
