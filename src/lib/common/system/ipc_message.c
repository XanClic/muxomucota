#include <ipc.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>
#include <sys/types.h>


void ipc_message(pid_t pid, int func_index, const void *buffer, size_t length)
{
    syscall4(SYS_IPC_MESSAGE, pid, func_index, (uintptr_t)buffer, length);
}
