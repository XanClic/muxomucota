#include <ipc.h>
#include <stdint.h>
#include <syscall.h>
#include <sys/types.h>


pid_t create_thread(void (*entry)(void *), void *stack_top, void *arg)
{
    return (pid_t)syscall3(SYS_CREATE_THREAD, (uintptr_t)entry, (uintptr_t)stack_top, (uintptr_t)arg);
}
