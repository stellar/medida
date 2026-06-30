// Optional Tracy integration wrapper.
#ifndef MEDIDA_TRACY_H_
#define MEDIDA_TRACY_H_

#if defined(__has_include)
#if __has_include(<Tracy.hpp>)
#include <Tracy.hpp>
#endif
#endif

#ifndef ZoneScoped
#define ZoneScoped ((void)0)
#endif

#endif // MEDIDA_TRACY_H_