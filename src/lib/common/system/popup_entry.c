#include <ipc.h>
#include <syscall.h>


void popup_entry(void (*entry)(int, uintptr_t))
{
    syscall1(SYS_POPUP_ENTRY, (uintptr_t)entry);
}
