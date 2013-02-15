#include <kassert.h>
#include <panic.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <string.h>


noreturn void format_panic(const char *format, ...)
{
    size_t max_len = 0;
    va_list va_count, va;

    va_start(va_count, format);
    va_copy(va, va_count);

    for (int i = 0; format[i]; i++)
    {
        if (format[i] != '%')
            max_len++;
        else
        {
            switch (format[++i])
            {
                case 0:
                    i--;
                case '%':
                    max_len++;
                    break;
                case 'i':
                    max_len += sizeof(int) * 3;
                    va_arg(va_count, int);
                    break;
                case 'p':
                    max_len += 2 + sizeof(uintptr_t) * 2;
                    va_arg(va_count, uintptr_t);
                    break;
                case 's':
                    max_len += strlen(va_arg(va_count, const char *));
                    break;
                default:
                    max_len += 2;
                    break;
            }
        }
    }

    va_end(va_count);


    char buffer[++max_len]; // Nullbyte
    int o = 0;

    for (int i = 0; format[i]; i++)
    {
        if (format[i] != '%')
            buffer[o++] = format[i];
        else
        {
            switch (format[++i])
            {
                case 0:
                    i--;
                case '%':
                    buffer[o++] = '%';
                    break;
                case 'i':
                {
                    int j = 12;
                    char tbuf[j];
                    tbuf[--j] = 0;

                    int val = va_arg(va, int);
                    bool neg = (val < 0);
                    if (neg)
                        val = -val;

                    if (!val)
                        tbuf[--j] = '0';

                    while (val)
                    {
                        tbuf[--j] = val % 10 + '0';
                        val /= 10;
                    }

                    if (neg)
                        tbuf[--j] = '-';

                    while (tbuf[j])
                        buffer[o++] = tbuf[j++];
                    break;
                }
                case 'p':
                {
                    buffer[o++] = '0';
                    buffer[o++] = 'x';

                    uintptr_t val = va_arg(va, uintptr_t);

                    for (int j = sizeof(uintptr_t) * 2 - 1; j >= 0; j--)
                    {
                        int digit = (val >> (j * 4)) & 0xf;
                        buffer[o++] = (digit > 9) ? (digit - 10 + 'a') : (digit + '0');
                    }

                    break;
                }
                case 's':
                {
                    const char *s = va_arg(va, const char *);
                    while (*s)
                        buffer[o++] = *(s++);
                    break;
                }
                default:
                    buffer[o++] = format[i - 1];
                    buffer[o++] = format[i];
            }
        }
    }

    buffer[o] = 0;


    panic(buffer);
}
