//
// Copyright (c) 2012 Daniel Lundin
//

#ifndef MEDIDA_SUMMARIZABLE_INTERFACE_H_
#define MEDIDA_SUMMARIZABLE_INTERFACE_H_

namespace medida {

#ifdef _MSC_VER
#ifdef min
#define msvc_min min
#undef min
#endif

#ifdef max
#define msvc_max max
#undef max
#endif
#endif

class SummarizableInterface {
public:
  virtual ~SummarizableInterface() {};
  virtual double max() const = 0;
  virtual double min() const = 0;
  virtual double mean() const = 0;
  virtual double std_dev() const = 0;
  virtual double sum() const = 0;
};

#ifdef _MSC_VER
#undef msvc_min
#undef msvc_max
#endif


} // namespace medida

#endif // MEDIDA_SUMMARIZABLE_INTERFACE_H_
