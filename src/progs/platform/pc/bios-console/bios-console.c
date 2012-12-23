#include <assert.h>
#include <drivers.h>
#include <drivers/memory.h>
#include <ipc.h>
#include <shm.h>
#include <stdint.h>
#include <string.h>
#include <vfs.h>


uint16_t *text_mem;


static uintptr_t vfs_open(void)
{
    return 1;
}


static uintptr_t vfs_close(void)
{
    return 0;
}


static uintptr_t vfs_dup(void)
{
    return 1;
}


static lock_t output_lock = unlocked;


static uintptr_t vfs_write(uintptr_t shmid)
{
    static int x, y;


    struct ipc_stream_send ipc_ss;

    int recv = popup_get_message(&ipc_ss, sizeof(ipc_ss));

    assert(recv == sizeof(ipc_ss));


    char *msg = shm_open(shmid, VMM_UR);

    lock(&output_lock);

    uint16_t *output = &text_mem[y * 80 + x];

    int i;
    for (i = 0; msg[i] && (i < (int)ipc_ss.size); i++)
    {
        switch (msg[i])
        {
            case '\n':
                y++;
            case '\r':
                x = 0;
                output = &text_mem[y * 80];
                break;
            default:
                *(output++) = msg[i] | 0x0700;
                if (++x >= 80)
                {
                    x -= 80;
                    y++;
                }
        }
    }

    unlock(&output_lock);

    shm_close(shmid, msg);

    return i;
}


int main(void)
{
    text_mem = map_memory(0xB8000, 80 * 25 * 2, VMM_UW);

    memset(text_mem, 0, 80 * 25 * 2);


    popup_message_handler(CREATE_PIPE,    vfs_open);
    popup_message_handler(DESTROY_PIPE,   vfs_close);
    popup_message_handler(DUPLICATE_PIPE, vfs_dup);

    popup_shm_handler    (STREAM_SEND,    vfs_write);


    daemonize("tty");
}
