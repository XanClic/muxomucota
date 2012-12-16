#include <ipc.h>
#include <syscall.h>


void popup_entry(void (*entry)(void))
{
    syscall1(SYS_POPUP_ENTRY, (uintptr_t)entry);
}
