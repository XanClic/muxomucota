#include <syscall.h>
#include <unistd.h>
#include <sys/types.h>


pid_t getpid(void)
{
    return syscall0(SYS_GETPID);
}
