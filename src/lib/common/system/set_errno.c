#include <errno.h>
#include <syscall.h>


void set_errno(int *errnop)
{
    syscall1(SYS_SET_ERRNO, (uintptr_t)errnop);
}
