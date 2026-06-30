//
// Copyright (c) 2012 Daniel Lundin
//

#ifndef MEDIDA_SAMPLE_H_
#define MEDIDA_SAMPLE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "medida/stats/snapshot.h"

namespace medida {
namespace stats {

class Sample {
public:
  virtual ~Sample() {};
  virtual void Clear() = 0;
  virtual std::uint64_t size() const = 0;
  virtual void Update(std::int64_t value) = 0;
  // Bulk update; implementations may amortize per-update overhead (locking,
  // clock reads) over the whole batch.
  virtual void UpdateMany(const std::vector<std::int64_t>& values) {
    for (auto value : values) {
      Update(value);
    }
  }
  virtual Snapshot MakeSnapshot(uint64_t divisor = 1) const = 0;
};

} // namespace stats
} // namespace medida

#endif // MEDIDA_SAMPLE_H_
