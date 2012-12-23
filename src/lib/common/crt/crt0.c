#include <errno.h>
#include <ipc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdnoreturn.h>

int errno;

extern int main(int argc, char *argv[], char *envp[]);

extern void _popup_ll_trampoline(int func_index, uintptr_t shmid);

noreturn void _start(int argc, char *argv[]);

noreturn void _start(int argc, char *argv[])
{
    set_errno(&errno);

    popup_entry(&_popup_ll_trampoline);

    exit(main(argc, argv, NULL));

    for (;;);
}
