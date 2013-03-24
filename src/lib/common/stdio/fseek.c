#include <errno.h>
#include <stdio.h>
#include <vfs.h>

int fseek(FILE *stream, long offset, int whence)
{
    switch (whence)
    {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            offset += pipe_get_flag(filepipe(stream), F_POSITION);
            break;
        case SEEK_END:
            offset += pipe_get_flag(filepipe(stream), F_FILESIZE);
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    return pipe_set_flag(filepipe(stream), F_POSITION, offset) ? 0 : -1;
}
