#ifndef _VFS_H
#define _VFS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


#define __MFILE 16

#define O_RDONLY (1 << 0)
#define O_WRONLY (1 << 1)
#define O_RDWR   (O_RDONLY | O_WRONLY)


typedef enum
{
    CREATE_PIPE,
    DESTROY_PIPE,
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

struct ipc_stream_send
{
    uintptr_t id;
    int flags;
    big_size_t size;
};


extern struct pipe _pipes[__MFILE];


int create_pipe(const char *path, int flags);
void destroy_pipe(int pipe, int flags);

big_size_t stream_send(int pipe, const void *data, big_size_t size, int flags);

#endif
