#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>

#ifndef KERNEL
#include <arch-syscall.h>
#endif


enum syscall_number
{
    SYS_EXIT, // 0
    SYS_SET_ERRNO,
    SYS_MAP_MEMORY,
    SYS_DAEMONIZE,
    SYS_POPUP_ENTRY,
    SYS_POPUP_EXIT,
    SYS_IPC_POPUP,
    SYS_IPC_POPUP_SYNC,
    SYS_POPUP_GET_MESSAGE, // 8
    SYS_FIND_DAEMON_BY_NAME,
    SYS_SHM_CREATE,
    SYS_SHM_MAKE,
    SYS_SHM_OPEN,
    SYS_SHM_CLOSE,
    SYS_SHM_UNMAKE,
    SYS_SHM_SIZE,
    SYS_SBRK, // 10
    SYS_YIELD,
    SYS_FORK
};


uintptr_t syscall_krn(int syscall_nr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, void *stack);

#endif
