#include <shm.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vmem.h>
#include <vfs.h>


int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: mount <file> <directory>\n");
        return 1;
    }

    int fd = create_pipe(argv[2], O_CREAT);

    if (fd < 0)
    {
        perror("Could not open file");
        return 1;
    }

    if (!pipe_implements(fd, I_FS))
    {
        destroy_pipe(fd, 0);
        fprintf(stderr, "Target directory does not implement the filesystem interface.\n");
        return 1;
    }


    big_size_t ret = stream_send(fd, argv[1], strlen(argv[1]) + 1, F_MOUNT_FILE);

    destroy_pipe(fd, 0);


    if (!ret)
        fprintf(stderr, "Could not mount.\n");

    return !ret;
}
