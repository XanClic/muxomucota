#include <errno.h>
#include <stddef.h>
#include <vfs.h>
#include <sys/stat.h>


int fstat(int fd, struct stat *buf)
{
    return pipe_stat(fd, buf);
}
