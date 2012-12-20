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
    SYS_IPC_POPUP,
    SYS_IPC_POPUP_SYNC,
    SYS_POPUP_GET_MESSAGE,
    SYS_FIND_DAEMON_BY_NAME,
    SYS_SHM_CREATE,
    SYS_SHM_MAKE,
    SYS_SHM_OPEN,
    SYS_SHM_CLOSE,
    SYS_SHM_UNMAKE,
    SYS_SHM_SIZE,
    SYS_SBRK,
    SYS_YIELD
};


#ifdef KERNEL
uintptr_t syscall5(int syscall_nr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4);
#endif

#endif
