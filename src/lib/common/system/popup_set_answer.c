#include <ipc.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>


void popup_set_answer(const void *buffer, size_t buflen)
{
    syscall2(SYS_POPUP_SET_ANSWER, (uintptr_t)buffer, buflen);
}
