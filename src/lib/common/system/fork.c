#include <syscall.h>
#include <sys/types.h>
#include <unistd.h>


pid_t fork(void)
{
    return syscall0(SYS_FORK);
}
