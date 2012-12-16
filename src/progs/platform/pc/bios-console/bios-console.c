#include <drivers.h>
#include <drivers/memory.h>
#include <ipc.h>
#include <stdint.h>


uint8_t *text_mem;


static void handler(void);

static void handler(void)
{
    text_mem[0] = 'X';
    text_mem[1] = 15;

    popup_exit();
}


int main(void)
{
    text_mem = map_memory(0xB8000, 80 * 25 * 2, VMM_UW);

    popup_entry(handler);

    daemonize();
}
