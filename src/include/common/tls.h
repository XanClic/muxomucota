#ifndef _TLS_H
#define _TLS_H

#include <arch-tls.h>


#ifdef KERNEL
void set_tls(struct tls *ptr);
#endif

#endif
