#ifndef _VFS_H
#define _VFS_H

#define __MFILE 16


typedef enum
{
    CREATE_PIPE,
    DESTROY_PIPE,
    STREAM_SEND
} vfs_function_t;


int create_pipe(const char *path, int flags);
void destroy_pipe(int pipe, int flags);

big_size_t stream_send(int pipe, const void *data, big_size_t size, int flags);

#endif
