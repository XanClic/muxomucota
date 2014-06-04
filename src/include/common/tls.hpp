#ifndef _TLS_HPP
#define _TLS_HPP


#include <sys/types.h>


struct tls {
    tls *absolute;

    pid_t pid, pgid, ppid;

    int errno;

#ifdef KERNEL
    void use(void);
#endif
};


#include <arch-tls.hpp>


#endif
