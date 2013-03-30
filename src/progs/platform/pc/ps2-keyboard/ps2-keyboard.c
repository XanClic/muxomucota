#include <drivers.h>
#include <drivers/ports.h>
#include <errno.h>
#include <ipc.h>
#include <keycodes.h>
#include <lock.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vfs.h>


static bool echo_keys = true;


static unsigned char *fifo_buffer;
static int fifo_read_idx = 0, fifo_write_idx = 0;
static lock_t fifo_lock = LOCK_INITIALIZER;

static void fifo_init(void)
{
    fifo_buffer = malloc(1024);
}

static void fifo_write(char c)
{
    lock(&fifo_lock);

    fifo_buffer[fifo_write_idx] = c;
    fifo_write_idx = (fifo_write_idx + 1) % 1024;

    // Bei Überlauf alte Einträge verwerfen
    if (fifo_write_idx == fifo_read_idx)
        fifo_read_idx = (fifo_read_idx + 1) % 1024;

    unlock(&fifo_lock);
}

static int fifo_read(void)
{
    lock(&fifo_lock);

    if (fifo_read_idx == fifo_write_idx)
    {
        unlock(&fifo_lock);
        return EOF;
    }

    int c = fifo_buffer[fifo_read_idx];
    fifo_read_idx = (fifo_read_idx + 1) % 1024;

    unlock(&fifo_lock);

    return c;
}

static bool fifo_eof(void)
{
    lock(&fifo_lock);

    if (fifo_read_idx == fifo_write_idx)
    {
        unlock(&fifo_lock);
        return false;
    }

    int c = fifo_buffer[fifo_read_idx];
    if (c == 255)
        fifo_read_idx = (fifo_read_idx + 1) % 1024;

    unlock(&fifo_lock);

    return c == 255;
}


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)relpath;

    if (flags & O_WRONLY)
    {
        errno = EACCES;
        return 0;
    }

    return 1;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    return id;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    (void)id;

    switch (flag)
    {
        case F_PRESSURE:
            return (fifo_write_idx + 1024 - fifo_read_idx) % 1024;
        case F_READABLE:
            return !fifo_eof();
        case F_WRITABLE:
            return false;
    }

    errno = EINVAL;
    return 0;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    (void)id;

    switch (flag)
    {
        case F_ECHO:
            echo_keys = value;
            return 0;
        case F_FLUSH:
            return 0;
    }

    errno = EINVAL;
    return -EINVAL;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)id;

    big_size_t got;
    char *out_stream = data;

    for (got = 0; got < size; got++)
    {
        int chr = fifo_read();
        if (chr == 255)
            break;
        else if (chr == EOF)
        {
            if (flags & O_NONBLOCK)
                break;
            else
            {
                yield();
                got--;
                continue;
            }
        }

        out_stream[got] = chr;
    }

    return got;
}


static void kbc_send_command(uint8_t command, int data);
static void kbd_send_command(uint8_t command, int data);
static void set_leds(int caps, int num, int scroll);
static void flush_input_queue(void);

// Infos zum letzten gesendeten Befehl
static int lc = 0, lcd = 0;


static int lshift = 0, rshift = 0, caps = 0, up = 0, ctrl = 0, menu = 0, meta = 0;

// Normales Layout
static unsigned kbd_n[128] =
{
    0, KEY_ESCAPE, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE, KEY_TAB,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', KEY_ENTER, KEY_CONTROL, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', KEY_LSHIFT, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', KEY_RSHIFT, '*', KEY_LMENU, ' ', KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_NUMLOCK, '*', '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', KEY_DELETE, 0, 0, '<', 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
// Layout mit Shift
static unsigned kbd_s[128] =
{
    0, KEY_ESCAPE, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', KEY_BACKSPACE, KEY_TAB,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', KEY_ENTER, KEY_CONTROL, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', KEY_LSHIFT, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', KEY_RSHIFT, '*', KEY_LMENU, ' ', KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_NUMLOCK, '*', '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', KEY_DELETE, 0, 0, '>', 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
// Gibt an, ob der Break-Code verarbeitet werden soll
static int kbd_b[128] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


static void __attribute__((regparm(1))) irq(void *info)
{
    (void)info;

    flush_input_queue();

    irq_handler_exit();
}


static void flush_input_queue(void)
{
    while (in8(0x64) & 1)
    {
        int scancode = in8(0x60);
        int ext = 0;
        int table_content = 0;

        switch (scancode)
        {
            case 0x55: // BAT beendet
            case 0xEE: // Testen der Verbindung
            case 0xFA: // Acknowledge
            case 0xFD: // BAT mit Fehler
                continue;
            case 0xFE: // Letzten Befehl nochmal senden
                kbd_send_command(lc, lcd);
                continue;
            case 0xE0:
                ext = 1;
                continue;
            case 0xE1:
                ext = 2;
                continue;
        }

        bool key_up = scancode >> 7;
        scancode &= 0x7F;

        switch (ext)
        {
            case 1:
                ext = 0;
                if (key_up && !kbd_b[scancode])
                    continue;

                switch (scancode)
                {
                    case 0x1C:
                        table_content = KEY_ENTER;
                        break;
                    case 0x1D:
                        table_content = KEY_RCONTROL;
                        break;
                    case 0x2A:
                        table_content = KEY_LSHIFT;
                        break;
                    case 0x35:
                        table_content = '/';
                        break;
                    case 0x36:
                        table_content = KEY_RSHIFT;
                        break;
                    case 0x38:
                        table_content = KEY_ALT_GR;
                        break;
                    case 0x47:
                        table_content = KEY_HOME;
                        break;
                    case 0x48:
                        table_content = KEY_UP;
                        break;
                    case 0x49:
                        table_content = KEY_PGUP;
                        break;
                    case 0x4B:
                        table_content = KEY_LEFT;
                        break;
                    case 0x4D:
                        table_content = KEY_RIGHT;
                        break;
                    case 0x4F:
                        table_content = KEY_END;
                        break;
                    case 0x50:
                        table_content = KEY_DOWN;
                        break;
                    case 0x51:
                        table_content = KEY_PGDOWN;
                        break;
                    case 0x52:
                        table_content = KEY_INSERT;
                        break;
                    case 0x53:
                        table_content = KEY_DELETE;
                        break;
                    default:
                        table_content = 0;
                }
                break;
            case 2:
                ext++;
                continue;
            case 3:
                ext = 0;
                continue;
            default:
                if (key_up && !kbd_b[scancode])
                    continue;

                if (!up)
                    table_content = kbd_n[scancode];
                else
                    table_content = kbd_s[scancode];
        }

        if (((table_content & 0xF000) != 0xE000) && table_content)
        {
            if (ctrl && (table_content == 'd'))
            {
                fifo_write(255);
                continue;
            }

            char echo[5] = { 0, 0, 0, 0, 0 };
            int length = 0;

            if (!(table_content & 0x80))
                length = 1;
            else if ((table_content & 0xE0) == 0xC0)
                length = 2;
            else if ((table_content & 0xF0) == 0xE0)
                length = 3;
            else if ((table_content & 0xF8) == 0xF0)
                length = 4;

            for (int i = 0; i < length; i++)
            {
                echo[i] = (table_content >> (i * 8)) & 0xFF;
                fifo_write(echo[i]);
            }

            if (echo_keys)
                printf("%s", echo);
        }
        else
        {
            switch (table_content)
            {
                case KEY_LSHIFT:
                    lshift = !key_up;
                    break;
                case KEY_RSHIFT:
                    rshift = !key_up;
                    break;
                case KEY_CAPSLOCK:
                    caps ^= 1;
                    set_leds(caps, 0, 0);
                    break;
                case KEY_LCONTROL:
                case KEY_RCONTROL:
                    ctrl ^= 1;
                    break;
                case KEY_LMETA:
                case KEY_RMETA:
                    meta ^= 1;
                    break;
                case KEY_MENU:
                    menu ^= 1;
                    break;
                case KEY_DELETE:
                    if (ctrl && menu)
                        kbc_send_command(0xFE, -1);
            }
            up = (lshift | rshift) ^ caps;
        }
    }
}


static void irq_thread(void *arg)
{
    (void)arg;

    register_irq_handler(1, &irq, NULL);

    daemonize("ps2-irq", false);
}


int main(void)
{
    fifo_init();


    iopl(3);


    create_thread(irq_thread, (void *)((uintptr_t)malloc(4096) + 4096), NULL);


    // Tastatur deaktivieren
    //kbd_send_command(0xF5, -1);

    set_leds(1, 1, 1);

    // Controller Command Byte setzen
    //kbc_send_command(0x60, 0x61);

    // Typematic rate
    kbd_send_command(0xF3, 0);
    // Scancodeset
    //kbd_send_command(0xF0, 0x01);

    // KBC aktivieren
    kbc_send_command(0xAE, -1);

    // Tastatur aktivieren
    kbd_send_command(0xF4, -1);

    set_leds(0, 0, 0);


    daemonize("kbd", true);
}


static inline void wait_for_transmit_free(void)
{
    while (in8(0x64) & 2);
}


static void kbc_send_command(uint8_t command, int data)
{
    wait_for_transmit_free();

    out8(0x64, command);

    if (!((uint32_t)data & ~0xFF))
    {
        wait_for_transmit_free();

        out8(0x60, data);
    }


    flush_input_queue();
}


static void kbd_send_command(uint8_t command, int data)
{
    wait_for_transmit_free();

    out8(0x60, command);

    if (!((uint32_t)data & ~0xFF))
    {
        wait_for_transmit_free();

        out8(0x60, data);
    }

    lc = command;
    lcd = data;


    flush_input_queue();
}


static void set_leds(int cled, int nled, int sled)
{
    kbd_send_command(0xED, ((!!cled) << 2) | ((!!nled) << 1) | (!!sled));
}
