#include <errno.h>
#include <ipc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdnoreturn.h>

int errno;

extern int main(int argc, char *argv[], char *envp[]);

extern void _popup_ll_trampoline(int func_index);

noreturn void _start(void);

noreturn void _start(void)
{
    set_errno(&errno);

    popup_entry((void (*)(void))&_popup_ll_trampoline);

    exit(main(0, NULL, NULL));

    for (;;);
}
