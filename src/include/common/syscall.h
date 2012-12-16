#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>

#ifndef KERNEL
#include <arch-syscall.h>
#endif


enum syscall_number
{
    SYS_EXIT,
    SYS_SET_ERRNO,
    SYS_MAP_MEMORY,
    SYS_DAEMONIZE,
    SYS_POPUP_ENTRY,
    SYS_POPUP_EXIT,
    SYS_IPC_MESSAGE
};


#ifdef KERNEL
uintptr_t syscall5(int syscall_nr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4);
#endif

#endif
