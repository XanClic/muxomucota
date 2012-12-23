#ifndef _VFS_H
#define _VFS_H

#include <proc-img-struct.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


#define __MFILE 32

#define O_RDONLY (1 << 0)
#define O_WRONLY (1 << 1)
#define O_RDWR   (O_RDONLY | O_WRONLY)


typedef enum
{
    CREATE_PIPE,
    DESTROY_PIPE,
    DUPLICATE_PIPE,
    STREAM_SEND
} vfs_function_t;


struct pipe
{
    pid_t pid;
    uintptr_t id;
};


struct ipc_create_pipe
{
    int flags;
    char relpath[];
};

struct ipc_destroy_pipe
{
    uintptr_t id;
    int flags;
};

struct ipc_duplicate_pipe
{
    uintptr_t id;
};

struct ipc_stream_send
{
    uintptr_t id;
    int flags;
    big_size_t size;
};


#define _pipes ((struct pipe *)_IMAGE_PIPE_ARRAY)


int create_pipe(const char *path, int flags);
void destroy_pipe(int pipe, int flags);

int duplicate_pipe(int pipe, int dest);
void duplicate_all_pipes(void);

big_size_t stream_send(int pipe, const void *data, big_size_t size, int flags);

#endif
