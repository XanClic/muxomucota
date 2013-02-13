#include <ipc.h>
#include <stdnoreturn.h>
#include <syscall.h>


noreturn void irq_handler_exit(void)
{
    syscall0(SYS_IRQ_HANDLER_EXIT);

    for (;;);
}
