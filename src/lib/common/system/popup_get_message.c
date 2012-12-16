#include <ipc.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>


size_t popup_get_message(void *buffer, size_t buflen)
{
    return syscall2(SYS_POPUP_GET_MESSAGE, (uintptr_t)buffer, buflen);
}
