#include <math.h>

#define __fmod(name, type) \
    type name(type x, type y) \
    { \
        type r; \
        if (isinf(x)) \
            return __domain_error_noerrno(); \
        if (y == 0.) \
            return __domain_error(); \
        __asm__ __volatile__ ("fprem" : "=t"(r) : "0"(x)); \
        return r; \
    }

__fmod(fmod, double)
__fmod(fmodf, float)
__fmod(fmodl, long double)
