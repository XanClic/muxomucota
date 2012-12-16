#include <ipc.h>
#include <syscall.h>
#include <sys/types.h>


pid_t find_daemon_by_name(const char *name)
{
    return syscall1(SYS_FIND_DAEMON_BY_NAME, (uintptr_t)name);
}
