/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#include <assert.h>
#include <drivers.h>
#include <drivers/memory.h>
#include <drivers/ports.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>
#include <vm86.h>


// #define WRONG

// #define STANDARD
#define SOLARIZED


#ifdef STANDARD

static const uint8_t palette[] = {
    0x00, 0x00, 0x00, // black
    0xcd, 0x00, 0x00, // red
    0x00, 0xcd, 0x00, // green
    0xcd, 0xcd, 0x00, // yellow
    0x00, 0x00, 0xee, // blue
    0xcd, 0x00, 0xcd, // magenta
    0x00, 0xcd, 0xcd, // cyan
    0xcd, 0xcd, 0xcd, // gray
    0x7f, 0x7f, 0x7f, // dark gray
    0xff, 0x00, 0x00, // light red
    0x00, 0xff, 0x00, // light green
    0xff, 0xff, 0x00, // light yellow
    0x5c, 0x5c, 0xff, // light blue
    0xff, 0x00, 0xff, // light magenta
    0x00, 0xff, 0xff, // light cyan
    0xff, 0xff, 0xff  // white
};

#define DEF_FG     7
#define DEF_BG     0
#define DEF_BR_FG 15

#define HAS_BRIGHT

#elif defined SOLARIZED

static const uint8_t palette[] = {
    0x07, 0x36, 0x42, // base02
    0xd3, 0x01, 0x02, // red
    0x85, 0x99, 0x00, // green
    0xb5, 0x89, 0x00, // yellow
    0x26, 0x8b, 0xd2, // blue
    0xd3, 0x36, 0x82, // magenta
    0x2a, 0xa1, 0x98, // cyan
    0xee, 0xe8, 0xd5, // white
    0x00, 0x2b, 0x36, // base03
    0xcb, 0x4b, 0x16, // orange
    0x58, 0x6e, 0x75, // base01
    0x65, 0x7b, 0x83, // base00
    0x83, 0x94, 0x96, // base0
    0x6c, 0x71, 0xc4, // violet
    0x93, 0xa1, 0xa1, // base1
    0xfd, 0xf6, 0xe3  // base3
};

#define DEF_FG    12
#define DEF_BG     8
#define DEF_BR_FG 15

#else
#error No color scheme specified.
#endif


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




static void load_palette(const uint8_t *pal)
{
    for (int i = 0; i < 16; i++)
    {
        out8(0x3c8, ((i < 8) && (i != 6)) ? i : ((i == 6) ? 20 : (48 + i)));
        out8(0x3c9, *(pal++) / 4);
        out8(0x3c9, *(pal++) / 4);
        out8(0x3c9, *(pal++) / 4);
    }

    // Blinken deaktivieren
    in8(0x3da);
    out8(0x3c0, 0x30);
    out8(0x3c0, in8(0x3c1) & ~0x08);
}


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)relpath;
    (void)flags;

    struct term_pipe *ntp = calloc(1, sizeof(struct term_pipe));

    ntp->fg = DEF_FG;
    ntp->bg = DEF_BG;

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


static lock_t output_lock = LOCK_INITIALIZER;


static void scroll_down(void)
{
    // FIXME, aber irgendwie auch nicht (tut ja so)

#ifdef WRONG
    uint32_t *dst = (uint32_t *)(text_mem + 80 * 25);
    uint32_t *src = (uint32_t *)(text_mem + 80 * 24);

    for (int i = 0; i < 960; i++)
        *(--dst) = *(--src);

    for (int i = 0; i < 40; i++)
        *(--dst) = 0x00200020 | (DEF_FG << 8) | (DEF_BG << 12) | (DEF_FG << 24) | (DEF_BG << 28);
#else
    uint32_t *dst = (uint32_t *)text_mem;
    uint32_t *src = (uint32_t *)(text_mem + 80);

    for (int i = 0; i < 960; i++)
        *(dst++) = *(src++);

    for (int i = 0; i < 40; i++)
        *(dst++) = 0x00200020 | (DEF_FG << 8) | (DEF_BG << 12) | (DEF_FG << 24) | (DEF_BG << 28);
#endif
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
#ifdef WRONG
    uint16_t tmp = (24 - y) * 80 + 79 - x;
#else
    uint16_t tmp = y * 80 + x;
#endif

    out8(0x3D4, 14);
    out8(0x3D5, tmp >> 8);
    out8(0x3D4, 15);
    out8(0x3D5, tmp);
}


static uint16_t *recalc_pos(int x, int y)
{
#ifdef WRONG
    return &text_mem[(24 - y) * 80 + 79 - x];
#else
    return &text_mem[y * 80 + x];
#endif
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

    uint16_t *output = recalc_pos(x, y);


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
                        if (!size--)
                            return omsz;
                        break;
                    case 'f':
                    case 'H':
                        type = ANSI_POSITION;
                        s++;
                        if (!size--)
                            return omsz;
                        break;
                    case 's':
                        type = ANSI_UNKNOWN;
                        tp->sx = x;
                        tp->sy = y;
                        s++;
                        if (!size--)
                            return omsz;
                        break;
                    case 'u':
                        type = ANSI_UNKNOWN;
                        x = tp->sx;
                        y = tp->sy;
                        output = recalc_pos(x, y);
                        s++;
                        if (!size--)
                            return omsz;
                        break;
                    default:
                        type = ANSI_UNKNOWN;
                        size += s - saved_pos - 1;
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
                                if (((*np != 'f') && (*np != 'H')) || (nx < 0) || (nx >= 80))
                                    continue;
                            }
                            x = nx;
                            y = ny;
                            output = recalc_pos(x, y);
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
                                            tp->fg = DEF_FG;
                                            tp->bg = DEF_BG;
                                            s++;
                                            subformat = -2;
                                            continue;
                                        case '1':
                                            if (tp->fg == DEF_FG)
                                                tp->fg = DEF_BR_FG;
#ifdef HAS_BRIGHT
                                            else
                                                tp->fg |= 8;
#endif
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
                                        subformat = -2;
                                        if ((*s >= '0') && (*s <= '7'))
#ifdef HAS_BRIGHT
                                            tp->fg = (*s - '0') | (tp->fg & 8);
#else
                                            tp->fg =  *s - '0';
#endif
                                        s++;
                                        continue;
                                    case '4':
                                        subformat = -2;
                                        if ((*s >= '0') && (*s <= '7'))
#ifdef HAS_BRIGHT
                                            tp->bg = (*s - '0') | (tp->bg & 8);
#else
                                            tp->bg =  *s - '0';
#endif
                                        s++;
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
                        if (!size--)
                            return omsz;
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
                output = recalc_pos(x, y);
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
                output = recalc_pos(x, y);
                break;
            case 0:
                break;
            case '\t':
                uni = ' '; // FIXME
            default:
#ifdef WRONG
                *(output--)
#else
                *(output++)
#endif
                    = get_c437_from_unicode(uni) | (tp->fg << 8) | (tp->bg << 12);

                if (++x >= 80)
                {
                    x -= 80;

                    if (++y >= 25)
                    {
                        scroll_down();
                        y--;

                        // Eine Zeile nach oben
#ifdef WRONG
                        output += 80;
#else
                        output -= 80;
#endif
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

    errno = EINVAL;
    return -EINVAL;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    (void)id;

    switch (flag)
    {
        case F_PRESSURE:
            return 0;
        case F_READABLE:
            return false;
        case F_WRITABLE:
            return true;
    }

    errno = EINVAL;
    return 0;
}


int main(void)
{
#ifdef WRONG
    void *full_mem = malloc(1048576);


    struct vm86_registers vm86_regs = { .eax = 0x1130, .ebx = 0x0600 };
    struct vm86_memory_area mem = { .vm = 0, .caller = full_mem, .size = 1048576, .in = false, .out = true };

    vm86_interrupt(0x10, &vm86_regs, &mem, 1);


    uint8_t *src = (uint8_t *)((uintptr_t)full_mem + ((vm86_regs.es & 0xffff) << 4) + (vm86_regs.ebp & 0xffff));
    uint8_t *dst = malloc(256 * 16);

    for (int c = 0; c < 256; c++)
    {
        for (int r = 0; r < 16; r++)
        {
            uint8_t swapped = src[c * 16 + 15 - r];

            swapped = ((swapped & 0x55) << 1) | ((swapped & 0xaa) >> 1);
            swapped = ((swapped & 0x33) << 2) | ((swapped & 0xcc) >> 2);
            swapped = ( swapped         << 4) | ( swapped         >> 4);

            dst[c * 16 + r] = swapped;
        }
    }


    vm86_regs.eax = 0x1100;
    vm86_regs.ebx = 16 << 8;
    vm86_regs.ecx = 256;
    vm86_regs.edx = 0x0000;
    vm86_regs.ebp = 0x0000;
    vm86_regs.es  = 0x1000;

    mem.vm = 0x10000;
    mem.caller = dst;
    mem.size = 256 * 16;
    mem.in = true;
    mem.out = false;

    vm86_interrupt(0x10, &vm86_regs, &mem, 1);


    free(dst);
    free(full_mem);


    // Cursorposition
    vm86_regs.eax = 0x0100;
    vm86_regs.ecx = 0x0001;

    vm86_interrupt(0x10, &vm86_regs, &mem, 1);
#endif


    // Für VGA-Operationen.
    iopl(3);


    load_palette(palette);


    text_mem = map_memory(0xB8000, 80 * 25 * 2, VMM_UW);

    for (int i = 0; i < 80 * 25; i++)
        text_mem[i] = (DEF_FG << 8) | (DEF_BG << 12);


    daemonize("tty", true);
}
