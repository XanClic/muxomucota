#include <assert.h>
#include <ipc.h>
#include <vfs.h>


extern uintmax_t _vfs_create_pipe(void);
extern uintmax_t _vfs_destroy_pipe(void);
extern uintmax_t _vfs_duplicate_pipe(void);

extern uintmax_t _vfs_stream_recv(uintptr_t shmid);
extern uintmax_t _vfs_stream_send(uintptr_t shmid);

extern uintmax_t _vfs_pipe_get_flag(void);
extern uintmax_t _vfs_pipe_set_flag(void);

extern uintmax_t _vfs_pipe_implements(void);


void _provide_vfs_services(void);

void _provide_vfs_services(void)
{
    popup_message_handler(CREATE_PIPE,     _vfs_create_pipe);
    popup_message_handler(DESTROY_PIPE,    _vfs_destroy_pipe);
    popup_message_handler(DUPLICATE_PIPE,  _vfs_duplicate_pipe);

    popup_shm_handler    (STREAM_RECV,     _vfs_stream_recv);
    popup_shm_handler    (STREAM_SEND,     _vfs_stream_send);

    popup_message_handler(PIPE_GET_FLAG,   _vfs_pipe_get_flag);
    popup_message_handler(PIPE_SET_FLAG,   _vfs_pipe_set_flag);

    popup_message_handler(PIPE_IMPLEMENTS, _vfs_pipe_implements);
}
