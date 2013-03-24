#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <vfs.h>

ssize_t read(int fd, void *buf, size_t count)
{
    return stream_recv(fd, buf, count, 0);
}
