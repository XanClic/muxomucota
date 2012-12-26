#include <ipc.h>
#include <stdnoreturn.h>
#include <syscall.h>


noreturn void popup_exit(uintmax_t exit_info)
{
    syscall1(SYS_POPUP_EXIT, (uintptr_t)&exit_info);

    for (;;);
}
