#include <stdint.h>
#include <stdlib.h>
#include <syscall.h>

void *sbrk(intptr_t diff)
{
    return (void *)syscall1(SYS_SBRK, (uintptr_t)diff);
}
