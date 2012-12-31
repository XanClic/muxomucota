#include <assert.h>
#include <ipc.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <vfs.h>


static void wait_for(const char *name)
{
    while (find_daemon_by_name(name) < 0)
        yield();
}


static void test_vfs(const char *filename)
{
    printf("Öffne %s.\n", filename);


    int fd = create_pipe(filename, O_RDONLY);

    if (fd < 0)
    {
        perror("Datei konnte nicht geöffnet werden");
        return;
    }


    size_t fsz = pipe_get_flag(fd, O_PRESSURE);
    char dst[fsz];

    stream_recv(fd, dst, fsz, 0);

    destroy_pipe(fd, 0);


    printf("Größe: %i B\n", (int)fsz);

    dst[fsz - 1] = 0; // Newline überschreiben (und nullterminieren sowieso)

    printf("Inhalt: \"%s\"\n", dst);
}


int main(int argc, char *argv[])
{
    const char *tty = NULL;

    for (int i = 0; i < argc; i++)
    {
        if (!strncmp(argv[i], "prime=", 6))
        {
            for (const char *tok = strtok(argv[i] + 6, ","); tok != NULL; tok = strtok(NULL, ","))
                wait_for(tok);
        }
        else if (!strncmp(argv[i], "tty=", 4))
            tty = argv[i] + 4;
    }

    if (tty == NULL)
        tty = "(tty)/tty0";

    assert(create_pipe(tty, O_RDONLY) == STDIN_FILENO);
    assert(create_pipe(tty, O_WRONLY) == STDOUT_FILENO);
    assert(create_pipe(tty, O_WRONLY) == STDERR_FILENO);


    printf("init läuft.\n\n");


    printf("Primordiale Services gestartet.\n");
    printf("stdin/stdout/stderr über %s.\n\n", tty);


    test_vfs("(mboot)/test.txt");


    return 0;
}
