#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <vfs.h>

FILE *fopen(const char *path, const char *mode)
{
    int imode = 0;

    switch (mode[0])
    {
        case 'r':
            imode = (mode[1] == '+') ? O_RDWR : O_RDONLY;
            break;
        case 'w':
            imode = (mode[1] == '+') ? O_RDWR | O_CREAT | O_TRUNC : O_WRONLY | O_CREAT | O_TRUNC;
            break;
        case 'a':
            imode = (mode[1] == '+') ? O_RDWR | O_APPEND : O_WRONLY | O_APPEND;
            break;
        default:
            errno = EINVAL;
            return NULL;
    }

    int fd = create_pipe(path, imode);
    if (fd < 0)
        return NULL;

    return fdopen(fd, mode);
}
