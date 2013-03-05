#include <ipc.h>
#include <syscall.h>


void register_irq_handler(int irq, void (__attribute__((regparm(1))) *handler)(void *info), void *info)
{
    syscall3(SYS_HANDLE_IRQ, irq, (uintptr_t)handler, (uintptr_t)info);
}
