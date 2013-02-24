#include <drivers.h>
#include <syscall.h>


extern void _provide_vfs_services(void);


noreturn void daemonize(const char *service, bool vfs)
{
    if (vfs)
        _provide_vfs_services();

    syscall1(SYS_DAEMONIZE, (uintptr_t)service);

    for (;;);
}
