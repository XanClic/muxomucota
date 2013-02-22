#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <vmem.h>
#include <sys/stat.h>
#include <sys/wait.h>

static char *cmdtok(char *s)
{
    static char *saved;

    if (s != NULL)
        saved = s;

    if (saved == NULL)
        return NULL;

    s = saved;

    while (!isspace(*saved) && *saved)
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
        if (*saved == '\'')
        {
            saved++;
            while ((*saved != '\'') && *saved)
                saved++;
            if (!*saved)
                break;
        }
        saved++;
    }

    if (isspace(*saved))
    {
        *saved = 0;
        while (isspace(*++saved));
    }

    if (!*saved)
        saved = NULL;

    return s;
}

// TODO: Tab Completion für builtins
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
            bool complete_cmd = (last_space == NULL);

            if (complete_cmd)
                last_space = s;
            else
                last_space++;

            char *path = getenv("PATH");

            char dirn[strlen(last_space) + strlen(path) + 2];
            char curpart[strlen(last_space) + 1];
            char ggt[1024];
            ggt[0] = 0;
            int got_count = 0;


            char duped_path[strlen(path) + 1];
            strcpy(duped_path, path);

            bool complete_from_path = (complete_cmd && (strchr(last_space, '/') == NULL));

            char *path_tok_base = duped_path;


            char *all_fitting = NULL;
            size_t all_fitting_sz = 0;

            bool is_dir = false, is_file = false;


            do
            {
                if (!complete_from_path)
                    strcpy(dirn, last_space);
                else
                {
                    char *tok = strtok(path_tok_base, ":");
                    path_tok_base = NULL;

                    if (tok == NULL)
                        break;

                    sprintf(dirn, "%s/%s", tok, last_space);
                }

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
                size_t this_len = strlen(curpart);

                while (buf[j] && (j < sizeof(buf)))
                {
                    if (!strncmp(buf + j, curpart, this_len))
                    {
                        size_t all_fitting_off = all_fitting_sz;

                        all_fitting_sz += strlen(buf + j) + 1;
                        all_fitting = realloc(all_fitting, all_fitting_sz);

                        strcpy(all_fitting + all_fitting_off, buf + j);

                        char full_name[strlen(dirn) + 1 + strlen(buf + j) + 1];
                        sprintf(full_name, "%s/%s", dirn, buf + j);

                        int fd = create_pipe(full_name, O_JUST_STAT);

                        if (fd >= 0)
                        {
                            if (pipe_implements(fd, I_STATABLE))
                            {
                                if (S_ISDIR(pipe_get_flag(fd, F_MODE)))
                                    is_dir  = true;
                                else
                                    is_file = true;
                            }
                            else
                                is_dir = is_file = true;

                            destroy_pipe(fd, 0);
                        }
                        else
                            is_dir = is_file = true;


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
            }
            while (complete_from_path);

            if (ggt[0])
            {
                for (size_t k = 0; ggt[k] && (i < size); k++)
                {
                    putchar(ggt[k]);
                    s[i++] = ggt[k];
                }
            }

            if (got_count < 2)
            {
                if (is_dir ^ is_file)
                {
                    char chr = is_dir ? '/' : ' ';

                    if (i < size)
                    {
                        putchar(chr);
                        s[i++] = chr;
                    }
                }
            }

            if ((got_count < 2) || ggt[0])
                continue;

            int j = 0;

            putchar('\n');
            while (j < (int)all_fitting_sz)
            {
                printf("%s ", all_fitting + j);
                j += strlen(all_fitting + j) + 1;
            }
            printf("\n%s$ %s", getenv("PWD"), s);

            free(all_fitting);

            continue;
        }
        else if (cc != '\b')
            putchar(cc);
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


extern void __simplify_path(char *path);

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


    curpath[0] = 0;

    if ((dir[0] != '/') && (dir[0] != '('))
    {
        const char *colon = strchr(dir, ':');
        const char *slash = strchr(dir, '/');

        if ((colon == NULL) || (slash == NULL) || (colon + 1 != slash))
        {
            strcpy(curpath, getenv("PWD"));
            strcat(curpath, "/");
        }
    }

    strcat(curpath, dir);

    if (chdir(curpath))
    {
        fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
        return 1;
    }

    // Gewusst, wie (__simplify_path)
    if ((curpath[0] == '/') || (curpath[0] == '('))
        __simplify_path(curpath);
    else
    {
        // bspw. "tar://blub", erst ab dem zweiten Slash arbeiten (sonst werden beide zusammengestrichen)
        __simplify_path(strchr(curpath, ':') + 2);
    }

    if (!curpath[0])
    {
        curpath[0] = '/';
        curpath[1] = 0;
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
