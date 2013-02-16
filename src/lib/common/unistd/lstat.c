#include <stddef.h>
#include <vfs.h>
#include <sys/stat.h>


int lstat(const char *path, struct stat *buf)
{
    return stat(path, buf); // FIXME
}
