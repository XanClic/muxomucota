#include <ipc.h>
#include <syscall.h>
#include <sys/types.h>


void ipc_message(pid_t pid)
{
    syscall1(SYS_IPC_MESSAGE, pid);
}
