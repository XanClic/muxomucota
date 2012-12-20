#include <ipc.h>
#include <syscall.h>


void yield(void)
{
    syscall0(SYS_YIELD);
}
