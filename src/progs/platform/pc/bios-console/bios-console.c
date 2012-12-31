#include <drivers.h>
#include <drivers/memory.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


uint16_t *text_mem;


extern uint8_t get_c437_from_unicode(unsigned unicode);


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)relpath;
    (void)flags;

    return 1;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)id;
    (void)flags;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    return id;
}


static lock_t output_lock = unlocked;


static void scroll_down(void)
{
    // FIXME, aber irgendwie auch nicht (tut ja so)

    uint32_t *dst = (uint32_t *)text_mem;
    uint32_t *src = (uint32_t *)(text_mem + 80);

    for (int i = 0; i < 960; i++)
        *(dst++) = *(src++);

    for (int i = 0; i < 40; i++)
        *(dst++) = 0x07200720;
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)id;
    (void)flags;


    static int x, y;
    static char saved[4];
    static int saved_len = 0, expected_len = 0;


    lock(&output_lock);

    uint16_t *output = &text_mem[y * 80 + x];


    const char *s = data;

    size_t omsz = size;

    while (*s && size)
    {
        unsigned uni = 0;
        const char *d = s;

        if (saved_len)
        {
            size_t rem = size;
            size += saved_len;
            while (*s && rem-- && (saved_len < expected_len))
                saved[saved_len++] = *(s++);

            if (saved_len < expected_len)
                break;

            d = saved;
        }
        if ((*d & 0xF8) == 0xF0)
        {
            if (size < 4)
            {
                memcpy(saved, s, size);
                saved_len = size;
                expected_len = 4;
                break;
            }
            uni = ((d[0] & 0x07) << 18) | ((d[1] & 0x3F) << 12) | ((d[2] & 0x3F) << 6) | (d[3] & 0x3F);
            d += 4;
            size -= 4;
        }
        else if ((*d & 0xF0) == 0xE0)
        {
            if (size < 3)
            {
                memcpy(saved, s, size);
                saved_len = size;
                expected_len = 3;
                break;
            }
            uni = ((d[0] & 0x0F) << 12) | ((d[1] & 0x3F) << 6) | (d[2] & 0x3F);
            d += 3;
            size -= 3;
        }
        else if ((*d & 0xE0) == 0xC0)
        {
            if (size < 2)
            {
                memcpy(saved, s, size);
                saved_len = size;
                expected_len = 2;
                break;
            }
            uni = ((d[0] & 0x1F) << 6) | (d[1] & 0x3F);
            d += 2;
            size -= 2;
        }
        else if (!(*d & 0x80))
        {
            uni = d[0];
            d++;
            size--;
        }
        else
        {
            uni = 0xFFFD;
            d++;
            size--;
        }

        if (!saved_len)
            s = d;
        else
            saved_len = 0;


        switch (uni)
        {
            case '\n':
                if (++y >= 25)
                {
                    scroll_down();
                    y--;
                }
            case '\r':
                x = 0;
                output = &text_mem[y * 80];
                break;
            case '\b':
                if (x > 0)
                    x--;
                else
                {
                    if (y)
                    {
                        x = 79;
                        y--;
                    }
                }
                output = &text_mem[y * 80 + x];
                break;
            case '\t':
                uni = ' '; // FIXME
            default:
                *(output++) = get_c437_from_unicode(uni) | 0x0700;
                if (++x >= 80)
                {
                    x -= 80;
                    if (++y >= 25)
                    {
                        scroll_down();
                        y--;
                    }
                }
        }
    }

    unlock(&output_lock);


    return omsz - size;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return ARRAY_CONTAINS((int[]){ I_TTY }, interface);
}


int main(void)
{
    text_mem = map_memory(0xB8000, 80 * 25 * 2, VMM_UW);

    memset(text_mem, 0, 80 * 25 * 2);


    daemonize("tty");
}
