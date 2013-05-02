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
#include <ipc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>


char **environ;
char *_cwd;

extern int main(int argc, char *argv[], char *envp[]);

extern void _popup_ll_trampoline(int func_index, uintptr_t shmid);

extern void _vfs_init(void);

noreturn void _start(int argc, char *argv[], char *envp[]);


noreturn void _start(int argc, char *argv[], char *envp[])
{
    popup_entry(&_popup_ll_trampoline);

    _vfs_init();


    int envc;
    for (envc = 0; envp[envc] != NULL; envc++);

    environ = malloc(sizeof(char *) * (envc + 1));
    memcpy(environ, envp, sizeof(char *) * (envc + 1));


    const char *cwd = getenv("_SYS_PWD");
    chdir((cwd == NULL) ? "/" : cwd);


    exit(main(argc, argv, envp));


    for (;;);
}
