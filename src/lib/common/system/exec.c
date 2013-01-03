#include <stdint.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>
#include <vfs.h>


int execvp(const char *file, char *const argv[])
{
    int fd = create_pipe(file, O_RDONLY);
    if (fd < 0)
        return -1;

    size_t len = pipe_get_flag(fd, F_PRESSURE);
    void *mem = malloc(len);
    stream_recv(fd, mem, len, 0);

    destroy_pipe(fd, 0);


    int ret = (int)syscall3(SYS_EXEC, (uintptr_t)mem, len, (uintptr_t)argv);

    free(mem);

    return ret;
}
