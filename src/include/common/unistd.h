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

#ifndef _UNISTD_H
#define _UNISTD_H

#include <errno.h>
#include <stddef.h>
#include <tls.h>

#ifdef KERNEL
#include <cpu-state.h>
#else
#include <syscall.h>
#endif
#include <sys/types.h>


#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2


#ifdef KERNEL
pid_t fork(struct cpu_state *current_state);
#else
pid_t fork(void);
#endif

int execvp(const char *file, char *const argv[]);
int execlp(const char *file, const char *arg, ...);

int chdir(const char *path);
char *getcwd(char *buf, size_t size);

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);


#ifndef KERNEL
static inline pid_t getpid(void)  { return __tls()->pid;  }
static inline pid_t getppid(void) { return __tls()->ppid; }

static inline pid_t getpgid(pid_t pid)
{
    if (!pid || (pid == getpid()))
        return __tls()->pgid;
    else
        return syscall1(SYS_GETPGID, pid);
}
#endif

#endif
