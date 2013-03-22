#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <cpu-state.h>
#include <stdint.h>

#ifndef KERNEL
#include <arch-syscall.h>
#endif


enum syscall_number
{
    SYS_EXIT, // 0
    SYS_MAP_MEMORY,
    SYS_UNMAP_MEMORY,
    SYS_DAEMONIZE,
    SYS_POPUP_ENTRY,
    SYS_POPUP_EXIT,
    SYS_IPC_POPUP,
    SYS_POPUP_GET_MESSAGE,
    SYS_FIND_DAEMON_BY_NAME, // 8
    SYS_SHM_CREATE,
    SYS_SHM_MAKE,
    SYS_SHM_OPEN,
    SYS_SHM_CLOSE,
    SYS_SHM_UNMAKE,
    SYS_SHM_SIZE,
    SYS_SBRK,
    SYS_YIELD, // 10
    SYS_FORK,
    SYS_EXEC,
    SYS_WAIT,
    SYS_HANDLE_IRQ,
    SYS_IRQ_HANDLER_EXIT,
    SYS_IOPL,
    SYS_PHYS_ALLOC,
    SYS_PHYS_FREE, // 18
    SYS_SLEEP,
    SYS_CREATE_THREAD,
    SYS_GETPGID,
    SYS_ELAPSED_MS,
    SYS_VM86
};


uintptr_t syscall_krn(int syscall_nr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, struct cpu_state *state);

#endif
