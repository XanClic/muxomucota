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

#ifndef _SYS__STAT_H
#define _SYS__STAT_H

#include <stdio.h>
#include <vfs.h>
#include <sys/types.h>

#define S_ISREG(m)  ((m) & S_IFREG)
#define S_ISDIR(m)  ((m) & S_IFDIR)
#define S_ISCHR(m)  ((m) & S_IFCHR)
#define S_ISBLK(m)  ((m) & S_IFBLK)
#define S_ISFIFO(m) ((m) & S_IFIFO)
#define S_ISLNK(m)  ((m) & S_IFLNK)
#define S_ISSOCK(m) ((m) & S_IFSOCK)
#define S_ISODEV(m) ((m) & S_IFODEV)
#define S_ISDEV(m)  ((m) & (S_IFCHR | S_IFBLK | S_IFODEV))

#define S_IFMT     03770000
#define S_IFODEV   02000000
#define S_IFSOCK   01000000
#define S_IFLNK    00400000
#define S_IFREG    00200000
#define S_IFBLK    00100000
#define S_IFDIR    00040000
#define S_IFCHR    00020000
#define S_IFIFO    00010000
#define S_ISUID    00004000
#define S_ISGID    00002000
#define S_ISVTX    00001000
#define S_IRWXU    00000700
#define S_IRUSR    00000400
#define S_IWUSR    00000200
#define S_IXUSR    00000100
#define S_IRWXG    00000070
#define S_IRGRP    00000040
#define S_IWGRP    00000020
#define S_IXGRP    00000010
#define S_IRWXO    00000007
#define S_IROTH    00000004
#define S_IWOTH    00000002
#define S_IXOTH    00000001

struct stat
{
    dev_t     st_dev;
    ino_t     st_ino;
    mode_t    st_mode;
    nlink_t   st_nlink;
    uid_t     st_uid;
    gid_t     st_gid;
    dev_t     st_rdev;
    off_t     st_size;
    blksize_t st_blksize;
    blkcnt_t  st_blocks;
    time_t    st_atime;
    time_t    st_mtime;
    time_t    st_ctime;
};

int stat(const char *path, struct stat *buf);
int lstat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
int pipe_stat(int p, struct stat *buf);

int chmod(const char *path, mode_t mode);
int fchmod(int fd, mode_t mode);
int mkdir(const char *pathname, mode_t mode);
int mkfifo(const char *pathname, mode_t mode);
int mknod(const char *filename, mode_t mode, dev_t device);

mode_t umask(mode_t mode);

#endif
