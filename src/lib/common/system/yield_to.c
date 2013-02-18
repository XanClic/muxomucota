#include <ipc.h>
#include <syscall.h>
#include <sys/types.h>


void yield_to(pid_t pid)
{
    syscall1(SYS_YIELD, pid);
}
