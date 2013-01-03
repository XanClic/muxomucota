#include <stddef.h>
#include <stdint.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/wait.h>


pid_t wait(int *stat_loc)
{
    return waitpid(-1, stat_loc, 0);
}


pid_t waitpid(pid_t pid, int *stat_loc, int options)
{
    uintmax_t exitval;

    pid_t retval = syscall3(SYS_WAIT, pid, (uintptr_t)&exitval, options);

    if ((stat_loc != NULL) && (retval > 0))
        *stat_loc = (int)exitval;

    return retval;
}
