#ifndef _TLS_H
#define _TLS_H


#include <sys/types.h>


struct tls
{
    struct tls *absolute;

    pid_t pid, pgid, ppid;

    int errno;
};


#include <arch-tls.h>


#ifdef KERNEL
void set_tls(struct tls *ptr);
#endif

#endif
