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
    printf("stdin/stdout/stderr über %s.\n", tty);


    printf("\nÖffne (mboot)/text.txt.\n");


    int fd = create_pipe("(mboot)/test.txt", O_RDONLY);

    char dst[64];
    size_t recvd = stream_recv(fd, dst, 64, 0);

    destroy_pipe(fd, 0);


    dst[recvd - 1] = 0; // Newline überschreiben (und nullterminieren sowieso)


    printf("Größe: %i B\n", recvd);

    printf("Inhalt: \"%s\"\n", dst);


    return 0;
}
