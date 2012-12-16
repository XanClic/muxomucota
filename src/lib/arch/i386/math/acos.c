#include <math.h>

#define __acos(name, type) \
    type name(type x) \
    { \
        if ((x > 1.) || (x < -1.)) \
            return __domain_error(); \
        if (isnan(x)) \
            return NAN; \
        return M_PI / 2 - __sign(x) * atan(x / (1 + sqrt(1 - (x * x)))); \
    }

__acos(acos, double)
__acos(acosf, float)
__acos(acosl, long double)
