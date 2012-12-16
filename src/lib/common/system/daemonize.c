#include <drivers.h>
#include <syscall.h>


noreturn void daemonize(void)
{
    syscall0(SYS_DAEMONIZE);

    for (;;);
}
