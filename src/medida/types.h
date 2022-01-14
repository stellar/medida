//
// Copyright (c) 2012 Daniel Lundin
//

#ifndef MEDIDA_TYPES_H_
#define MEDIDA_TYPES_H_

#include <chrono>

namespace medida {

  using Clock = std::chrono::steady_clock;
  using SystemClock = std::chrono::system_clock;

} // namespace medida

#endif // MEDIDA_TYPES_H_
