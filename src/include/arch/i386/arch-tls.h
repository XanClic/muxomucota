#ifndef _ARCH_TLS_H
#define _ARCH_TLS_H

#include <compiler.h>


#ifndef KERNEL
static inline struct tls *__tls(void) cc_pure;

static inline struct tls *__tls(void)
{
    struct tls *tls;
    __asm__ __volatile__ ("mov %%fs:(0),%0" : "=r"(tls));
    return tls;
}
#endif

#endif
