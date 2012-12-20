#include <assert.h>
#include <drivers.h>
#include <drivers/memory.h>
#include <ipc.h>
#include <shm.h>
#include <stdint.h>
#include <string.h>
#include <vfs.h>


uint8_t *text_mem;


static uintptr_t vfs_open(void)
{
    return 1;
}


static uintptr_t vfs_close(void)
{
    return 0;
}


static uintptr_t vfs_write(uintptr_t shmid)
{
    struct ipc_stream_send ipc_ss;

    int recv = popup_get_message(&ipc_ss, sizeof(ipc_ss));

    assert(recv == sizeof(ipc_ss));


    char *msg = shm_open(shmid, VMM_UR);

    int i;
    for (i = 0; msg[i] && (i < (int)ipc_ss.size); i++)
    {
        text_mem[2 * i    ] = msg[i];
        text_mem[2 * i + 1] = 7;
    }

    shm_close(shmid, msg);

    return i;
}


int main(void)
{
    text_mem = map_memory(0xB8000, 80 * 25 * 2, VMM_UW);

    memset(text_mem, 0, 80 * 25 * 2);


    popup_message_handler(CREATE_PIPE,  vfs_open);
    popup_message_handler(DESTROY_PIPE, vfs_close);

    popup_shm_handler    (STREAM_SEND,  vfs_write);


    daemonize("tty");
}
