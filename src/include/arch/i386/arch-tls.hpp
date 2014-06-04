#ifndef _ARCH_TLS_HPP
#define _ARCH_TLS_HPP

#include <compiler.hpp>


#ifndef KERNEL
static inline tls *__tls(void) cxx_pure;

static inline tls *__tls(void)
{
    tls *t;
    __asm__ __volatile__ ("mov %%fs:(0),%0" : "=r"(tls));
    return t;
}
#endif

#endif
