#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vfs.h>

int fclose(FILE *stream)
{
    int fd = stream->fd;
    free(stream);

    destroy_pipe(fd, 0);

    return 0;
}
