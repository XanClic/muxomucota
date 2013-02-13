#include <drivers/ports.h>
#include <syscall.h>


void iopl(int level)
{
    syscall1(SYS_IOPL, level);
}
