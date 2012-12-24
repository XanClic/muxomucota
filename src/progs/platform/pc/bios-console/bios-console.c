#include <assert.h>
#include <drivers.h>
#include <drivers/memory.h>
#include <ipc.h>
#include <shm.h>
#include <stdint.h>
#include <string.h>
#include <vfs.h>


uint16_t *text_mem;


extern uint8_t get_c437_from_unicode(unsigned unicode);


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


static uintptr_t vfs_write(uintptr_t shmid)
{
    static int x, y;
    static char saved[4];
    static int saved_len = 0, expected_len = 0;


    struct ipc_stream_send ipc_ss;

    int recv = popup_get_message(&ipc_ss, sizeof(ipc_ss));

    assert(recv == sizeof(ipc_ss));


    const char *msg = shm_open(shmid, VMM_UR);

    lock(&output_lock);

    uint16_t *output = &text_mem[y * 80 + x];


    const char *s = msg;

    size_t msz = ipc_ss.size, omsz = msz;

    while (*s && msz)
    {
        unsigned uni = 0;
        const char *d = s;

        if (saved_len)
        {
            size_t rem = msz;
            msz += saved_len;
            while (*s && rem-- && (saved_len < expected_len))
                saved[saved_len++] = *(s++);

            if (saved_len < expected_len)
                break;

            d = saved;
        }
        if ((*d & 0xF8) == 0xF0)
        {
            if (msz < 4)
            {
                memcpy(saved, s, msz);
                saved_len = msz;
                expected_len = 4;
                break;
            }
            uni = ((d[0] & 0x07) << 18) | ((d[1] & 0x3F) << 12) | ((d[2] & 0x3F) << 6) | (d[3] & 0x3F);
            d += 4;
            msz -= 4;
        }
        else if ((*d & 0xF0) == 0xE0)
        {
            if (msz < 3)
            {
                memcpy(saved, s, msz);
                saved_len = msz;
                expected_len = 3;
                break;
            }
            uni = ((d[0] & 0x0F) << 12) | ((d[1] & 0x3F) << 6) | (d[2] & 0x3F);
            d += 3;
            msz -= 3;
        }
        else if ((*d & 0xE0) == 0xC0)
        {
            if (msz < 2)
            {
                memcpy(saved, s, msz);
                saved_len = msz;
                expected_len = 2;
                break;
            }
            uni = ((d[0] & 0x1F) << 6) | (d[1] & 0x3F);
            d += 2;
            msz -= 2;
        }
        else if (!(*d & 0x80))
        {
            uni = d[0];
            d++;
            msz--;
        }
        else
        {
            uni = 0xFFFD;
            d++;
            msz--;
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

    shm_close(shmid, msg);


    return omsz - msz;
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
