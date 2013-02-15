#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <vmem.h>
#include <sys/wait.h>

static char *cmdtok(char *s)
{
    static char *saved;

    if (s != NULL)
        saved = s;

    if (saved == NULL)
        return NULL;

    s = saved;

    while ((*saved != ' ') && *saved)
    {
        if (*saved == '\\')
        {
            if (!*(++saved))
                break;
        }
        if (*saved == '"')
        {
            saved++;
            while ((*saved != '"') && *saved)
                saved++;
            if (!*saved)
                break;
        }
        saved++;
    }

    if (!*saved)
        saved = NULL;
    else
        *(saved++) = 0;

    return s;
}

static char *my_fgets(char *s, int size, int stream)
{
    int cc = 0, i = 0;

    memset(s, 0, size);

    size--;
    while ((i < size) && (cc != '\n'))
    {
        fflush(stdout);

        stream_recv(stream, &cc, 1, O_BLOCKING);

        if (cc == '\t')
        {
            const char *last_space = strrchr(s, ' ');
            if (last_space != NULL)
                last_space++;
            else
                last_space = s;

            char dirn[strlen(last_space) + 1];
            char curpart[strlen(last_space) + 1];
            char ggt[1024];
            ggt[0] = 0;

            strcpy(dirn, last_space);

            char *last_slash = strrchr(dirn, '/');
            if (last_slash != NULL)
            {
                strcpy(curpart, last_slash + 1);
                last_slash[1] = 0;
            }
            else
            {
                if (dirn[0] == '(')
                    continue;
                strcpy(curpart, dirn);
                strcpy(dirn, ".");
            }

            int dir = create_pipe(dirn, O_RDONLY);
            if (dir < 0)
                continue;

            char buf[pipe_get_flag(dir, F_PRESSURE)];
            stream_recv(dir, buf, sizeof(buf), 0);
            destroy_pipe(dir, 0);

            size_t j = 0;
            int got_count = 0;
            size_t this_len = strlen(curpart);

            while (buf[j] && (j < sizeof(buf)))
            {
                if (!strncmp(buf + j, curpart, this_len))
                {
                    if (!got_count++)
                        strcpy(ggt, buf + j + this_len);
                    else
                    {
                        size_t k = 0;
                        do
                        {
                            if (ggt[k] != buf[j + this_len + k])
                            {
                                ggt[k] = 0;
                                break;
                            }
                        }
                        while (ggt[k++]);
                    }
                }
                j += strlen(buf + j) + 1;
            }

            if (ggt[0])
            {
                for (size_t k = 0; ggt[k] && (i < size); k++)
                {
                    printf("%c", ggt[k]);
                    s[i++] = ggt[k];
                }
                continue;
            }

            if (got_count < 2)
                continue;

            j = 0;

            printf("\n");
            while (buf[j] && (j < sizeof(buf)))
            {
                if (!strncmp(buf + j, curpart, this_len))
                    printf("%s ", buf + j);
                j += strlen(buf + j) + 1;
            }
            printf("\n%s$ %s", getenv("PWD"), s);

            continue;
        }
        else if (cc != '\b')
            printf("%c", cc);
        else
        {
            if (i)
            {
                printf("\b \b");

                while (((s[--i] & 0xC0) == 0x80) && (i > 1))
                    s[i] = 0;
                s[i] = 0;
            }

            continue;
        }

        if (cc != '\n')
            s[i++] = cc;
    }

    return s;
}

static int kill_me(void)
{
    const char *status = cmdtok(NULL);
    if (status != NULL)
        exit(atoi(status));
    exit(0);
}

static int echo_help(void)
{
#ifdef I386
    puts("Paloxena 3.5 shell on i386-µxoµcota.");
#else
    puts("Paloxena 3.5 shell on µxoµcota (unknown architecture).");
#endif

    puts("Built-in commands:");
    puts(" exit: Exits the shell.");
    puts(" help: Prints this list.");

    return 0;
}

static int change_dir(void)
{
    const char *dir = cmdtok(NULL);
    if (dir == NULL)
    {
        dir = getenv("HOME");
        if (dir == NULL)
            dir = "/";
    }

    size_t total_sz = strlen(getenv("PWD")) + 1 + strlen(dir) + 1;
    char curpath[total_sz];

    if (dir[0] == '/')
        curpath[0] = 0;
    else
    {
        strcpy(curpath, getenv("PWD"));
        strcat(curpath, "/");
    }

    strcat(curpath, dir);

    if (chdir(dir))
    {
        fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
        return 1;
    }

    for (char *part = strchr(curpath, '/'); part != NULL; part = strchr(part + 1, '/'))
    {
        while (part[1] == '/')
            memmove(&part[1], &part[2], &curpath[total_sz] - part - 2);

        if (!part[1])
        {
            part[0] = 0;
            break;
        }

        while ((part[1] == '.') && (part[2] == '/'))
            memmove(&part[1], &part[3], &curpath[total_sz] - part - 3);

        if ((part[1] == '.') && !part[2])
        {
            part[0] = 0;
            break;
        }

        while ((part[1] == '.') && (part[2] == '.') && ((part[3] == '/') || !part[4]))
        {
            char *prev = part - 1;
            while ((prev > curpath) && (*prev != '/'))
                prev--;

            if (*prev != '/')
                break;

            if (part[4])
            {
                memmove(&prev[1], &part[4], &curpath[total_sz] - part - 4);
                part = prev;
            }
            else
            {
                prev[0] = 0;
                part = prev;
                break;
            }
        }
    }

    setenv("OLDPWD", getenv("PWD"), 1);
    setenv("PWD", curpath, 1);

    return 0;
}


static int clear(void)
{
    printf("\033[2J\033[H");
    fflush(stdout);

    return 0;
}


struct
{
    const char name[8];
    int (*function)(void);
} builtins[] =
{
    {
        .name = "exit",
        .function = &kill_me
    },
    {
        .name = "help",
        .function = &echo_help
    },
    {
        .name = "cd",
        .function = &change_dir
    },
    {
        .name = "clear",
        .function = &clear
    }
};

static int cmd_called(char *cmd)
{
    if (!*cmd || (*cmd == '#'))
        return 0;

    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
        if (!strcmp(cmd, builtins[i].name))
            return builtins[i].function();

    // FIXME
    const char *argv[32];
    bool in_background = false;

    argv[0] = cmd;
    for (int i = 1; i < 31; i++)
    {
        argv[i] = cmdtok(NULL);
        if (argv[i] == NULL)
        {
            if ((i > 1) && !strcmp(argv[i - 1], "&"))
            {
                argv[i - 1] = NULL;
                in_background = true;
            }
            break;
        }
    }
    argv[31] = NULL;

    int exit_code;

    pid_t pid = fork();
    if (pid < 0)
        perror("Could not fork");
    else if (!pid)
    {
        execvp(cmd, (char *const *)argv);
        perror(cmd);
        exit(1);
    }

    if (in_background)
        exit_code = 0;
    else
    {
        waitpid(pid, &exit_code, 0);
        if (WIFEXITED(exit_code))
            exit_code = WEXITSTATUS(exit_code);
        else
            exit_code = 1;
    }

    return exit_code;
}

int main(void)
{
    char input[128];

    pipe_set_flag(STDIN_FILENO, F_ECHO, 0);

    if (getenv("PWD") != NULL)
        chdir(getenv("PWD"));
    else
    {
        char cwd[256];
        if (getcwd(cwd, 256) == NULL)
        {
            chdir("/");
            setenv("PWD", "/", 1);
        }
        else
        {
            chdir(cwd);
            setenv("PWD", cwd, 1);
        }
    }

    for (;;)
    {
        printf("%s$ ", getenv("PWD"));

        fflush(stdout);

        my_fgets(input, 128, STDIN_FILENO);

        cmd_called(cmdtok(input));
    }
}
