#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <vfs.h>


extern char **environ;


int execvp(const char *file, char *const argv[])
{
    int fd = -1;

    if (strchr(file, '/') != NULL)
        fd = create_pipe(file, O_RDONLY);
    else
    {
        const char *cpvar = getenv("PATH");
        if (cpvar == NULL)
            cpvar = ".:/bin:/usr/bin";

        char *npath = malloc(strlen(file) + strlen(cpvar) + 2);
        char *pvar = strdup(cpvar);

        STRTOK_FOREACH(pvar, ":", pp)
        {
            strcpy(npath, pp);
            if (npath[strlen(npath) - 1] != '/')
                strcat(npath, "/");

            strcat(npath, file);

            fd = create_pipe(npath, O_RDONLY);
            if (fd >= 0)
                break;
        }

        free(npath);
        free(pvar);
    }

    if (fd < 0)
        return -1;


    size_t len = pipe_get_flag(fd, F_PRESSURE);
    void *mem = malloc(len);
    stream_recv(fd, mem, len, 0);

    destroy_pipe(fd, 0);


    int ret = (int)syscall4(SYS_EXEC, (uintptr_t)mem, len, (uintptr_t)argv, (uintptr_t)environ);

    free(mem);

    return ret;
}
