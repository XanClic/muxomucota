#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdnoreturn.h>

int errno;

extern int main(int argc, char *argv[], char *envp[]);

noreturn void _start(void);

noreturn void _start(void)
{
    set_errno(&errno);

    exit(main(0, NULL, NULL));

    for (;;);
}
