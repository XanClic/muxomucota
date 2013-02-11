#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <vfs.h>


int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        int fd = filepipe(stdin);

        while (pipe_get_flag(fd, F_READABLE))
        {
            size_t press = pipe_get_flag(fd, F_PRESSURE);
            char inp[press];

            stream_recv(fd, inp, press, 0);
            stream_send(filepipe(stdout), inp, press, 0);
        }

        return 0;
    }


    for (int i = 1; i < argc; i++)
    {
        int fd = create_pipe(argv[i], O_RDONLY);

        if (fd < 0)
        {
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
            continue;
        }

        while (pipe_get_flag(fd, F_READABLE))
        {
            size_t press = pipe_get_flag(fd, F_PRESSURE);
            char inp[press];

            stream_recv(fd, inp, press, 0);
            stream_send(filepipe(stdout), inp, press, 0);
        }
    }


    return 0;
}
