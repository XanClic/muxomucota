#include <ipc.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>
#include <sys/types.h>


void ipc_message(pid_t pid, int func_index, const void *buffer, size_t length)
{
    syscall5(SYS_IPC_POPUP, pid, func_index, 0, (uintptr_t)buffer, length);
}


uintptr_t ipc_message_synced(pid_t pid, int func_index, const void *buffer, size_t length)
{
    return syscall5(SYS_IPC_POPUP_SYNC, pid, func_index, 0, (uintptr_t)buffer, length);
}


void ipc_shm(pid_t pid, int func_index, uintptr_t shmid)
{
    syscall5(SYS_IPC_POPUP, pid, func_index, shmid, 0, 0);
}


uintptr_t ipc_shm_synced(pid_t pid, int func_index, uintptr_t shmid)
{
    return syscall5(SYS_IPC_POPUP_SYNC, pid, func_index, shmid, 0, 0);
}


void ipc_shm_message(pid_t pid, int func_index, uintptr_t shmid, const void *buffer, size_t length)
{
    syscall5(SYS_IPC_POPUP, pid, func_index, shmid, (uintptr_t)buffer, length);
}


uintptr_t ipc_shm_message_synced(pid_t pid, int func_index, uintptr_t shmid, const void *buffer, size_t length)
{
    return syscall5(SYS_IPC_POPUP_SYNC, pid, func_index, shmid, (uintptr_t)buffer, length);
}
