#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>
#include <sys/stat.h>


static void print_info(const char *path, const char *name)
{
    struct stat stt;

    if (stat(path, &stt) < 0)
        printf("??????????    ?    ?        ? %s\n", name);
    else
    {
        printf("%c", S_ISDIR(stt.st_mode) ? 'd' : '-');
        printf("%c%c%c", stt.st_mode & S_IRUSR ? 'r' : '-', stt.st_mode & S_IWUSR ? 'w' : '-', stt.st_mode & S_IXUSR ? 'x' : '-');
        printf("%c%c%c", stt.st_mode & S_IRGRP ? 'r' : '-', stt.st_mode & S_IWGRP ? 'w' : '-', stt.st_mode & S_IXGRP ? 'x' : '-');
        printf("%c%c%c", stt.st_mode & S_IROTH ? 'r' : '-', stt.st_mode & S_IWOTH ? 'w' : '-', stt.st_mode & S_IXOTH ? 'x' : '-');
        printf(" %4i %4i %8lli ", (int)stt.st_uid, (int)stt.st_gid, (long long)stt.st_size);
        if (S_ISDIR(stt.st_mode) && S_ISDEV(stt.st_mode))
            printf("\33[0;31;1m");
        else if (S_ISDIR(stt.st_mode))
            printf("\33[0;34;1m");
        else if (S_ISDEV(stt.st_mode))
            printf("\33[0;33;1m");
        printf("%s\33[0m\n", name);
    }
}


static void list(const char *cmd, const char *dir)
{
    int d = create_pipe(dir, O_JUST_STAT);
    if (d < 0)
    {
        fprintf(stderr, "%s: %s: %s\n", cmd, dir, strerror(errno));
        return;
    }

    if (!pipe_implements(d, I_STATABLE))
    {
        destroy_pipe(d, 0);
        fprintf(stderr, "%s: %s: No information available\n", cmd, dir);
        return;
    }

    if (!S_ISDIR(pipe_get_flag(d, F_MODE)))
    {
        destroy_pipe(d, 0);

        char *duped_dir = strdup(dir);
        print_info(dir, basename(duped_dir));
        free(duped_dir);

        return;
    }


    destroy_pipe(d, 0);

    d = create_pipe(dir, O_RDONLY);
    if (d < 0)
    {
        fprintf(stderr, "%s: %s: %s\n", cmd, dir, strerror(errno));
        return;
    }


    size_t len = pipe_get_flag(d, F_PRESSURE);
    if (!len)
        return;

    char *entries = malloc(len);
    stream_recv(d, entries, len, 0);

    char *pmem, *pmemp;

    pmem = malloc(strlen(dir) + 256 + 2);
    strcpy(pmem, dir);
    strcat(pmem, "/");

    pmemp = &pmem[strlen(pmem)];

    pmem[strlen(dir) + 256 + 1] = 0;

    unsigned i = 0;
    while (entries[i] && (i < len))
    {
        strncpy(pmemp, &entries[i], 256);

        print_info(pmem, &entries[i]);

        i += strlen(&entries[i]) + 1;
    }

    free(entries);
    free(pmem);

    destroy_pipe(d, 0);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        list(argv[0], ".");
    else
    {
        for (int i = 1; i < argc; i++)
            list(argv[0], argv[i]);
    }

    return 0;
}
