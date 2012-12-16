#include <ipc.h>
#include <stdnoreturn.h>
#include <syscall.h>


noreturn void popup_exit(void)
{
    syscall0(SYS_POPUP_EXIT);

    for (;;);
}
