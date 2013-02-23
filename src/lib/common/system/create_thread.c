#include <ipc.h>
#include <syscall.h>


int create_thread(void (*entry)(void *), void *stack_top, void *arg)
{
    return (int)syscall3(SYS_CREATE_THREAD, (uintptr_t)entry, (uintptr_t)stack_top, (uintptr_t)arg);
}
