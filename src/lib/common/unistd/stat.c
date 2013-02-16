#include <stddef.h>
#include <vfs.h>
#include <sys/stat.h>


int stat(const char *path, struct stat *buf)
{
    int p = create_pipe(path, O_JUST_STAT);
    if (p < 0)
        return -1;

    int ret = pipe_stat(p, buf);
    destroy_pipe(p, 0);

    return ret;
}
