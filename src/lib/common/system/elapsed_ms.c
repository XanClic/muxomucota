#include <syscall.h>
#include <system-timer.h>


int elapsed_ms(void)
{
    return syscall0(SYS_ELAPSED_MS);
}
