#ifndef _VFS_H
#define _VFS_H

#include <proc-img-struct.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


#define __MFILE 32

// create_pipe-Flags
#define O_RDONLY      (1 << 0)
#define O_WRONLY      (1 << 1)
#define O_RDWR        (O_RDONLY | O_WRONLY)
#define O_CREAT       (1 << 2)
#define O_JUST_STAT   (1 << 3)
#define O_TRUNC       (1 << 4)
#define O_APPEND      (1 << 5)
#define O_NOFOLLOW    (1 << 6)

// stream_send/stream_recv-Flags
#define O_NONBLOCK    (1 << 0)
#define O_BLOCKING    !O_NONBLOCK


enum
{
#include <vfs/interfaces/file.h>
#include <vfs/interfaces/fs.h>
#include <vfs/interfaces/statable.h>
#include <vfs/interfaces/tty.h>
};

// pipe_(g/s)et_flag()-Standardflags
enum
{
    F_PRESSURE,
    F_READABLE,
    F_WRITABLE,
    F_FLUSH,

#include <vfs/flags/file.h>
#include <vfs/flags/fs.h>
#include <vfs/flags/statable.h>
#include <vfs/flags/tty.h>
};


typedef enum
{
    CREATE_PIPE,
    DESTROY_PIPE,
    DUPLICATE_PIPE,

    STREAM_SEND,
    STREAM_RECV,

    PIPE_GET_FLAG,
    PIPE_SET_FLAG,

    PIPE_IMPLEMENTS,

    IS_SYMLINK
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

struct ipc_stream_recv
{
    uintptr_t id;
    int flags;
    big_size_t size;
};

struct ipc_pipe_get_flag
{
    uintptr_t id;
    int flag;
};

struct ipc_pipe_set_flag
{
    uintptr_t id;
    int flag;
    uintmax_t value;
};

struct ipc_pipe_implements
{
    uintptr_t id;
    int interface;
};


#define _pipes ((struct pipe *)_IMAGE_PIPE_ARRAY)


int create_pipe(const char *path, int flags);
void destroy_pipe(int pipe, int flags);

int duplicate_pipe(int pipe, int dest);
void duplicate_all_pipes(void);

big_size_t stream_send(int pipe, const void *data, big_size_t size, int flags);
big_size_t stream_recv(int pipe, void *data, big_size_t size, int flags);

uintmax_t pipe_get_flag(int pipe, int flag);
int pipe_set_flag(int pipe, int flag, uintmax_t value);

bool pipe_implements(int pipe, int interface);


uintptr_t service_create_pipe(const char *relpath, int flags);
void service_destroy_pipe(uintptr_t id, int flags);
uintptr_t service_duplicate_pipe(uintptr_t id);

big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags);
big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags);

uintmax_t service_pipe_get_flag(uintptr_t id, int flag);
int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value);

bool service_pipe_implements(uintptr_t id, int interface);

bool service_is_symlink(const char *relpath, char *linkpath);

#endif
