#include <syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <vfs.h>


pid_t fork(void)
{
    pid_t child_pid = syscall0(SYS_FORK);

    if (!child_pid)
    {
        // Im Kindprozess gibt es auch im Userspace noch was zu forken
        duplicate_all_pipes();
    }

    return child_pid;
}
