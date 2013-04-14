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
    char *npath = NULL, *fullpath = (char *)file;

    if (strchr(file, '/') != NULL)
        fd = create_pipe(file, O_RDONLY);
    else
    {
        const char *cpvar = getenv("PATH");
        if (cpvar == NULL)
            cpvar = ".:/bin:/usr/bin";

        npath = malloc(strlen(file) + strlen(cpvar) + 2);
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

        free(pvar);

        fullpath = npath;
    }

    if (fd < 0)
    {
        free(npath);
        return -1;
    }


    size_t len = pipe_get_flag(fd, F_PRESSURE);
    void *mem = malloc(len);
    stream_recv(fd, mem, len, 0);

    destroy_pipe(fd, 0);


    int argc;
    for (argc = 0; argv[argc] != NULL; argc++);

    char *n_argv[argc + 2];
    n_argv[0] = NULL;
    memcpy(n_argv + 1, argv, (argc + 1) * sizeof(char *));

    uintptr_t argvp = (uintptr_t)&n_argv[1];


    if ((len > 2) && (((char *)mem)[0] == '#') && (((char *)mem)[1] == '!'))
    {
        char *interpreter = strtok((char *)mem + 2, "\r\n");

        fd = create_pipe(interpreter, O_RDONLY);
        if (fd < 0)
        {
            free(mem);
            return -1;
        }

        n_argv[0] = strdup(interpreter);
        n_argv[1] = fullpath;
        argvp = (uintptr_t)n_argv;

        free(mem);

        len = pipe_get_flag(fd, F_PRESSURE);
        mem = malloc(len);
        stream_recv(fd, mem, len, 0);

        destroy_pipe(fd, 0);
    }


    int ret = (int)syscall4(SYS_EXEC, (uintptr_t)mem, len, argvp, (uintptr_t)environ);

    free(n_argv[0]);
    free(mem);
    free(npath);

    return ret;
}
