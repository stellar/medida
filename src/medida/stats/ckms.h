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
  CKMS();
  explicit CKMS(const std::vector<Quantile>& quantiles);

  void insert(double value);
  double get(double q);
  void reset();
  std::size_t count() const;
  double max() const;

 private:
  double allowableError(int rank);
  bool insertBatch();
  void compress();

 private:
  const std::reference_wrapper<const std::vector<Quantile>> quantiles_;

  std::size_t count_;
  std::vector<Item> sample_;
  std::array<double, 500> buffer_;
  std::size_t buffer_count_;
  std::size_t size_when_last_sorted_;

  double max_;
};

} // namespace stats
} // namespace medida
