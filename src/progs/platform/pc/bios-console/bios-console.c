#include <drivers.h>
#include <drivers/memory.h>
#include <drivers/ports.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


uint16_t *text_mem;


struct term_pipe
{
    char saved[4];
    int saved_len;
    int expected_len;
    int sx, sy;
    int fg, bg;
};


extern uint8_t get_c437_from_unicode(unsigned unicode);


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)relpath;
    (void)flags;

    struct term_pipe *ntp = calloc(1, sizeof(struct term_pipe));

    ntp->fg = 7;
    ntp->bg = 0;

    return (uintptr_t)ntp;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    free((void *)id);
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct term_pipe *ntp = malloc(sizeof(*ntp));
    memcpy(ntp, (struct term_pipe *)id, sizeof(*ntp));

    return (uintptr_t)ntp;
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

static void clrscr(int fg, int bg)
{
    uint32_t *dst = (uint32_t *)text_mem;

    uint32_t mask = 0x20 | (bg << 12) | (fg << 8);
    mask = mask | (mask << 16);

    for (int i = 0; i < 1000; i++)
        *(dst++) = mask;
}


static void display_cursor(uint8_t x, uint8_t y)
{
    uint16_t tmp = y * 80 + x;

    out8(0x3D4, 14);
    out8(0x3D5, tmp >> 8);
    out8(0x3D4, 15);
    out8(0x3D5, tmp);
}


typedef enum
{
    ANSI_UNKNOWN,
    ANSI_COLOR,
    ANSI_POSITION
} ansi_type;

big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;


    static int x, y;
    ansi_type type;
    int subformat;
    const char *saved_pos;


    struct term_pipe *tp = (struct term_pipe *)id;


    lock(&output_lock);

    uint16_t *output = &text_mem[y * 80 + x];


    const char *s = data;

    size_t omsz = size;

    while (size)
    {
        unsigned uni = 0;
        const char *d = s;

        if (tp->saved_len)
        {
            size_t rem = size;
            size += tp->saved_len;
            while (*s && rem-- && (tp->saved_len < tp->expected_len))
                tp->saved[tp->saved_len++] = *(s++);

            if (tp->saved_len < tp->expected_len)
                break;

            d = tp->saved;
        }
        if ((*d & 0xF8) == 0xF0)
        {
            if (size < 4)
            {
                memcpy(tp->saved, s, size);
                tp->saved_len = size;
                tp->expected_len = 4;
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
                memcpy(tp->saved, s, size);
                tp->saved_len = size;
                tp->expected_len = 3;
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
                memcpy(tp->saved, s, size);
                tp->saved_len = size;
                tp->expected_len = 2;
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

        if (!tp->saved_len)
            s = d;
        else
            tp->saved_len = 0;


        switch (uni)
        {
            case 27:
                if (*s != '[')
                    break;
                s++;
                if (!size--)
                    return omsz;
                saved_pos = s;
                for (;;)
                {
                    if (((*s >= 64) && (*s <= 126)) || !*s)
                        break;
                    s++;
                    if (!size--)
                        return omsz;
                }
                switch (*s)
                {
                    case 'm':
                        type = ANSI_COLOR;
                        break;
                    case 'J':
                        type = ANSI_UNKNOWN;
                        if (*saved_pos == '2')
                            clrscr(tp->fg, tp->bg);
                        s++;
                        break;
                    case 'f':
                    case 'H':
                        type = ANSI_POSITION;
                        s++;
                        break;
                    case 's':
                        type = ANSI_UNKNOWN;
                        tp->sx = x;
                        tp->sy = y;
                        s++;
                        break;
                    case 'u':
                        type = ANSI_UNKNOWN;
                        x = tp->sx;
                        y = tp->sy;
                        s++;
                        break;
                    default:
                        type = ANSI_UNKNOWN;
                        s = saved_pos - 1;
                        break;
                }
                if (type == ANSI_UNKNOWN)
                    break;
                switch ((int)type)
                {
                    case ANSI_POSITION:
                        {
                            int nx, ny;
                            const char *np = saved_pos;
                            if (np == s - 1)
                                nx = ny = 0;
                            else
                            {
                                ny = strtol(np, (char **)&np, 10) - 1;
                                if ((*np != ';') || (ny < 0) || (ny >= 25))
                                    continue;
                                nx = strtol(np + 1, (char **)&np, 10) - 1;
                                if ((*np != 'f') || (nx < 0) || (nx >= 80))
                                    continue;
                            }
                            x = nx;
                            y = ny;
                            break;
                        }
                    case ANSI_COLOR:
                        s = saved_pos;
                        subformat = -1;
                        for (;;)
                        {
                            if ((*s >= '0') && (*s <= '9'))
                            {
                                if (subformat == -1)
                                {
                                    switch (*s)
                                    {
                                        case '0':
                                            tp->fg = 7;
                                            tp->bg = 0;
                                            s++;
                                            subformat = -2;
                                            continue;
                                        case '1':
                                            tp->fg |= 8;
                                            s++;
                                            subformat = -2;
                                            continue;
                                    }
                                }
                                if (subformat == -1)
                                {
                                    subformat = *s;
                                    s++;
                                    continue;
                                }
                                if (subformat == -2)
                                {
                                    s++;
                                    continue;
                                }
                                switch (subformat)
                                {
                                    case '2':
                                        subformat = -2;
                                        switch (*(s++))
                                        {
                                            case '2':
                                                tp->fg &= 7;
                                                continue;
                                        }
                                        continue;
                                    case '3':
                                        tp->fg &= 8;
                                        subformat = -2;
                                        switch (*(s++))
                                        {
                                            case '0':
                                                continue;
                                            case '1':
                                                tp->fg |= 4;
                                                continue;
                                            case '2':
                                                tp->fg |= 2;
                                                continue;
                                            case '3':
                                                tp->fg |= 6;
                                                continue;
                                            case '4':
                                                tp->fg |= 1;
                                                continue;
                                            case '5':
                                                tp->fg |= 5;
                                                continue;
                                            case '6':
                                                tp->fg |= 3;
                                                continue;
                                            case '7':
                                                tp->fg |= 7;
                                                continue;
                                        }
                                        continue;
                                    case '4':
                                        tp->bg &= 8;
                                        subformat = -2;
                                        switch (*(s++))
                                        {
                                            case '0':
                                                continue;
                                            case '1':
                                                tp->bg |= 4;
                                                continue;
                                            case '2':
                                                tp->bg |= 2;
                                                continue;
                                            case '3':
                                                tp->bg |= 6;
                                                continue;
                                            case '4':
                                                tp->bg |= 1;
                                                continue;
                                            case '5':
                                                tp->bg |= 5;
                                                continue;
                                            case '6':
                                                tp->bg |= 3;
                                                continue;
                                            case '7':
                                                tp->bg |= 7;
                                                continue;
                                        }
                                        continue;
                                }
                            }
                            if (*s == 'm')
                                break;
                            if (*s == ';')
                                subformat = -1;
                            s++;
                        }
                        s++;
                        break;
                }
                break;
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
            case 0:
                break;
            case '\t':
                uni = ' '; // FIXME
            default:
                *(output++) = get_c437_from_unicode(uni) | (tp->fg << 8) | (tp->bg << 12);

                if (++x >= 80)
                {
                    x -= 80;

                    if (++y >= 25)
                    {
                        scroll_down();
                        y--;

                        output -= 80; // Eine Zeile nach oben
                    }
                }
        }
    }


    display_cursor(x, y);


    unlock(&output_lock);


    return omsz - size;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return ARRAY_CONTAINS((int[]){ I_TTY }, interface);
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    (void)id;
    (void)value;

    switch (flag)
    {
        case F_FLUSH:
            return 0;
    }

    return -EINVAL;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    (void)id;

    switch (flag)
    {
        case F_READABLE:
            return false;
        case F_WRITABLE:
            return true;
    }

    return 0;
}


int main(void)
{
    // Für display_cursor().
    iopl(3);


    text_mem = map_memory(0xB8000, 80 * 25 * 2, VMM_UW);

    for (int i = 0; i < 80 * 25; i++)
        text_mem[i] = 0x0700;


    daemonize("tty");
}
