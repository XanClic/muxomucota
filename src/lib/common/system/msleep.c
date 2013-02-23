#include <ipc.h>
#include <syscall.h>


void msleep(int ms)
{
    syscall1(SYS_SLEEP, ms);
}
