#include <stdlib.h>
#include <stdnoreturn.h>
#include <syscall.h>


noreturn void exit(int status)
{
    syscall1(SYS_EXIT, status);

    for (;;);
}
