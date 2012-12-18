#include <ipc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>
#include <sys/types.h>


void ipc_message(pid_t pid, int func_index, const void *buffer, size_t length)
{
    syscall5(SYS_IPC_MESSAGE, pid, func_index, (uintptr_t)buffer, length, false);
}


uintptr_t ipc_message_synced(pid_t pid, int func_index, const void *buffer, size_t length)
{
    return syscall5(SYS_IPC_MESSAGE, pid, func_index, (uintptr_t)buffer, length, true);
}


void ipc_shm(pid_t pid, int func_index, uintptr_t shmid)
{
    syscall4(SYS_IPC_SHM, pid, func_index, shmid, false);
}


uintptr_t ipc_shm_synced(pid_t pid, int func_index, uintptr_t shmid)
{
    return syscall4(SYS_IPC_SHM, pid, func_index, shmid, true);
}
