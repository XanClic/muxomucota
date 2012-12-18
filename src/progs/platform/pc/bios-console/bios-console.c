#include <drivers.h>
#include <drivers/memory.h>
#include <ipc.h>
#include <shm.h>
#include <stdint.h>
#include <string.h>


uint8_t *text_mem;


static uintptr_t handler(uintptr_t shmid);

static uintptr_t handler(uintptr_t shmid)
{
    int incoming = shm_size(shmid);

    char *msg = shm_open(shmid, VMM_UR);

    for (int i = 0; msg[i] && (i < incoming); i++)
    {
        text_mem[2 * i    ] = msg[i];
        text_mem[2 * i + 1] = 7;
    }

    shm_close(shmid, msg);

    return 0;
}


int main(void)
{
    text_mem = map_memory(0xB8000, 80 * 25 * 2, VMM_UW);

    memset(text_mem, 0, 80 * 25 * 2);

    popup_shm_handler(0, handler);

    daemonize("tty");
}
