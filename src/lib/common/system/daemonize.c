#include <drivers.h>
#include <syscall.h>


noreturn void daemonize(const char *service)
{
    syscall1(SYS_DAEMONIZE, (uintptr_t)service);

    for (;;);
}
