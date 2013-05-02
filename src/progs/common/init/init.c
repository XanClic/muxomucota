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

#include <assert.h>
#include <errno.h>
#include <ipc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vfs.h>


static void wait_for(const char *name)
{
    while (find_daemon_by_name(name) < 0)
        yield();
}


int main(int argc, char *argv[])
{
    if (getpid() != 1)
    {
        fprintf(stderr, "%s: No init commands available.\n", argv[0]);
        return 1;
    }


    const char *tty = NULL, *script = NULL;

    for (int i = 0; i < argc; i++)
    {
        if (!strncmp(argv[i], "prime=", 6))
            for (const char *tok = strtok(argv[i] + 6, ","); tok != NULL; tok = strtok(NULL, ","))
                wait_for(tok);
        else if (!strncmp(argv[i], "tty=", 4))
            tty = argv[i] + 4;
        else if (!strncmp(argv[i], "script=", 7))
            script = argv[i] + 7;
    }


    if (tty == NULL)
        tty = "(tty)/tty0";

    int infd, outfd, errfd;

    infd  = create_pipe(tty, O_RDONLY);
    outfd = create_pipe(tty, O_WRONLY);
    errfd = create_pipe(tty, O_WRONLY);

    assert(infd  == STDIN_FILENO);
    assert(outfd == STDOUT_FILENO);
    assert(errfd == STDERR_FILENO);


    printf("init running.\n\n");


    printf("Primordial servers running.\n");
    printf("stdin/stdout/stderr via %s.\n\n", tty);


    setenv("PATH", "/bin", 1);

    chdir("/");
    setenv("PWD", "/", 1);
    setenv("OLDPWD", "/", 1);


    if (script == NULL)
    {
        fprintf(stderr, "No init script given.\n");
        return 1;
    }


    printf("Executing %s.\n\n", script);


    int fd = create_pipe(script, O_RDONLY);

    if (fd < 0)
    {
        perror("Could not open init script");
        return 1;
    }


    size_t len = pipe_get_flag(fd, F_PRESSURE);

    char *commands = malloc(len + 1);

    stream_recv(fd, commands, len, 0);
    commands[len] = 0;

    destroy_pipe(fd, 0);

    int status_line_length = 0;


    STRTOK_FOREACH(commands, "\n", line)
    {
        if (line[0] == '#')
            continue;


        int c_argc;
        const char *l = line;
        for (c_argc = 1; *l; l++)
            if (*l == ' ')
                c_argc++;

        char *c_argv[c_argc + 1];

        int i = 0;
        STRTOK_FOREACH(line, " ", arg)
            c_argv[i++] = arg;
        c_argv[i] = NULL;


        if (!strcmp(c_argv[0], "stdin"))
        {
            destroy_pipe(STDIN_FILENO, 0);
            infd = create_pipe(c_argv[1], O_RDONLY);
            assert(infd == STDIN_FILENO);
        }
        else if (!strcmp(c_argv[0], "@echo"))
        {
            if (!strcmp(c_argv[1], "off"))
            {
                destroy_pipe(STDOUT_FILENO, 0);

                outfd = create_pipe("/dev/null", O_WRONLY);
                assert(outfd == STDOUT_FILENO);
            }
            else if (!strcmp(c_argv[1], "on"))
            {
                destroy_pipe(STDOUT_FILENO, 0);

                outfd = create_pipe(tty, O_WRONLY);
                assert(outfd == STDOUT_FILENO);
            }
        }
        else if (!strcmp(c_argv[0], "begin"))
        {
            for (int j = 1; j < c_argc; j++)
            {
                printf("%s ", c_argv[j]);
                status_line_length += strlen(c_argv[j]) + 1;
            }
            fflush(stdout);
        }
        else if (!strcmp(c_argv[0], "done"))
        {
            printf("%*c\033[1m[\033[32mDONE\033[0;1m]\033[0m\n", 73 - status_line_length, ' ');
            status_line_length = 0;
        }
        else
        {
            pid_t child = fork();
            if (!child)
            {
                execvp(c_argv[0], c_argv);
                exit(errno);
            }


            int status;
            waitpid(child, &status, 0);
        }
    }


    free(commands);


    return 0;
}
