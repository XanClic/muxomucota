#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>


FILE *stdin  = &(FILE){ .fd = STDIN_FILENO  };
FILE *stdout = &(FILE){ .fd = STDOUT_FILENO };
FILE *stderr = &(FILE){ .fd = STDERR_FILENO };


void _vfs_init(void);

void _vfs_init(void)
{
}


void _vfs_deinit(void);

void _vfs_deinit(void)
{
    for (int i = 0; i < __MFILE; i++)
        destroy_pipe(i, 0);
}
