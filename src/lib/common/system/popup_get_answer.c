#include <ipc.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>


size_t popup_get_answer(uintptr_t answer_id, void *buffer, size_t buflen)
{
    return syscall3(SYS_POPUP_GET_ANSWER, answer_id, (uintptr_t)buffer, buflen);
}
