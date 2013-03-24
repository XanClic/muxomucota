#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <vfs.h>

ssize_t write(int fd, const void *buf, size_t count)
{
    return stream_send(fd, buf, count, 0);
}
