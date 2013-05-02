/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

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
