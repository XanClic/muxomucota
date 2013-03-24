#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vfs.h>

FILE *fdopen(int fd, const char *mode)
{
    if ((fd < 0) || (fd >= __MFILE))
    {
        errno = EBADF;
        return NULL;
    }

    FILE *fp = malloc(sizeof(*fp));
    fp->fd = fd;
    fp->bufsz = 0;
    fp->eof = false;

    (void)mode;

    return fp;
}
