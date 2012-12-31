#include <drivers.h>
#include <syscall.h>


extern void _provide_vfs_services(void);


noreturn void daemonize(const char *service)
{
    _provide_vfs_services();

    syscall1(SYS_DAEMONIZE, (uintptr_t)service);

    for (;;);
}
