#ifdef assert
#undef assert
#endif

#ifndef NDEBUG

#include <stdio.h>
#include <stdlib.h>


#ifndef __quote
#define __squote(arg) #arg
#define __quote(arg) __squote(arg)
#endif

#define assert(assertion) do { if (!(assertion)) { printf(__FILE__ ":" __quote(__LINE__) ": "); printf(__func__); printf(": Assertion '" #assertion "' failed.\n"); abort(); } } while (0)

#endif
