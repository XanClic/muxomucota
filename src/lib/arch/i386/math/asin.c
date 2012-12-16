#include <math.h>

#define __asin(name, type) \
    type name(type x) \
    { \
        if ((x > 1.) || (x < -1.)) \
            return __domain_error(); \
        if (isnan(x)) \
            return NAN; \
        return 2 * atan(x / (1 + sqrt(1 - (x * x)))); \
    }

__asin(asin, double)
__asin(asinf, float)
__asin(asinl, long double)
