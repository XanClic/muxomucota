#include <errno.h>
#include <syscall.h>
#include <vfs.h>
#include <sys/stat.h>


int pipe_stat(int p, struct stat *buf)
{
    if ((p < 0) || (p >= __MFILE) || !_pipes[p].pid)
    {
        errno = EBADF;
        return -1;
    }

    if (!pipe_implements(p, I_STATABLE))
    {
        errno = ENOTSUP;
        return -1;
    }

    buf->st_dev     = _pipes[p].pid;
    buf->st_ino     = pipe_get_flag(p, F_INODE);
    buf->st_mode    = pipe_get_flag(p, F_MODE);
    buf->st_nlink   = pipe_get_flag(p, F_NLINK);
    buf->st_uid     = pipe_get_flag(p, F_UID);
    buf->st_gid     = pipe_get_flag(p, F_GID);
    buf->st_rdev    = 0;
    buf->st_size    = pipe_get_flag(p, pipe_implements(p, I_FILE) ? F_FILESIZE : F_PRESSURE);
    buf->st_blksize = pipe_get_flag(p, F_BLOCK_SIZE);
    buf->st_blocks  = pipe_get_flag(p, F_BLOCK_COUNT);
    buf->st_atime   = pipe_get_flag(p, F_ATIME);
    buf->st_mtime   = pipe_get_flag(p, F_MTIME);
    buf->st_ctime   = pipe_get_flag(p, F_CTIME);

    return 0;
}
