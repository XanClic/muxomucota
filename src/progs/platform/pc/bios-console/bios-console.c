#include <drivers.h>
#include <drivers/memory.h>
#include <ipc.h>
#include <stdint.h>
#include <string.h>


uint8_t *text_mem;


static uintptr_t handler(void);

static uintptr_t handler(void)
{
    size_t incoming = popup_get_message(NULL, 0);

    char msg[incoming + 1];
    popup_get_message(msg, incoming);

    msg[incoming] = 0;

    for (int i = 0; msg[i]; i++)
    {
        text_mem[2 * i    ] = msg[i];
        text_mem[2 * i + 1] = 7;
    }

    return 0;
}


int main(void)
{
    text_mem = map_memory(0xB8000, 80 * 25 * 2, VMM_UW);

    memset(text_mem, 0, 80 * 25 * 2);

    popup_handler(0, handler);

    daemonize("tty");
}
