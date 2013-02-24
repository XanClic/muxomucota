#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int asprintf(char **strp, const char *format, ...)
{
    va_list args1, args2;
    int rval;

    va_start(args1, format);
    va_copy(args2, args1);
    rval = vsnprintf(NULL, 0, format, args1);
    va_end(args1);

    *strp = malloc(rval + 1);
    vsnprintf(*strp, rval + 1, format, args2);
    va_end(args2);

    return rval;
}
