#include <errno.h>
#include <ipc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>


int errno;
char **environ;
char *_cwd;

extern int main(int argc, char *argv[], char *envp[]);

extern void _popup_ll_trampoline(int func_index, uintptr_t shmid);

extern void _vfs_init(void);

noreturn void _start(int argc, char *argv[], char *envp[]);


noreturn void _start(int argc, char *argv[], char *envp[])
{
    set_errno(&errno);

    popup_entry(&_popup_ll_trampoline);

    _vfs_init();


    int envc;
    for (envc = 0; envp[envc] != NULL; envc++);

    environ = malloc(sizeof(char *) * (envc + 1));
    memcpy(environ, envp, sizeof(char *) * (envc + 1));


    const char *cwd = getenv("_SYS_PWD");
    chdir((cwd == NULL) ? "/" : cwd);


    exit(main(argc, argv, envp));


    for (;;);
}
