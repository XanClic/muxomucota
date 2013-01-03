#include <stdlib.h>
#include <stdnoreturn.h>
#include <syscall.h>


void _vfs_deinit(void);


noreturn void exit(int status)
{
    _vfs_deinit();

    syscall1(SYS_EXIT, status);

    for (;;);
}
