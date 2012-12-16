#include <stddef.h>
#include <stdint.h>
#include <string.h>


void *memset(void *s, int c, size_t n)
{
    uint8_t *d = s;

    while (n--)
        *(d++) = c;

    return s;
}


void *memsetptr(void *s, uintptr_t c, size_t n)
{
    uintptr_t *d = s;

    while (n--)
        *(d++) = c;

    return s;
}


void *memcpy(void *restrict d, const void *restrict s, size_t n)
{
    uint8_t *restrict dst = d;
    const uint8_t *restrict src = s;

    while (n--)
        *(dst++) = *(src++);

    return d;
}


char *strcpy(char *restrict d, const char *restrict s)
{
    uint8_t *restrict dst = (uint8_t *)d;
    const uint8_t *restrict src = (const uint8_t *)s;

    do
        *(dst++) = *src;
    while (*(src++));

    return d;
}


char *strncpy(char *restrict d, const char *restrict s, size_t n)
{
    uint8_t *restrict dst = (uint8_t *)d;
    const uint8_t *restrict src = (const uint8_t *)s;

    while (n && *src)
    {
        *(dst++) = *(src++);
        n--;
    }

    while (n--)
        *(dst++) = 0;

    return d;
}


int strncmp(const char *restrict s1, const char *restrict s2, size_t n)
{
    while (n--)
    {
        if (*(s1++) != *s2)
            return (unsigned char)s2[0] - (unsigned char)s1[-1];

        if (!*(s2++))
            return 0;
    }

    return 0;
}
