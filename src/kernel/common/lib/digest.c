#include <digest.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


static digest_t crc32(const void *buffer, size_t size)
{
    digest_t rem_poly = 0xffffffffu;
    const uint8_t *bp = buffer;

    while (size--)
    {
#ifdef LITTLE_ENDIAN
        rem_poly ^= *(bp++);

        for (int i = 0; i < 8; i++)
            if (rem_poly & 1)
                rem_poly = (rem_poly >> 1) ^ 0xedb88320;
            else
                rem_poly >>= 1;
#else
#error Missing non-little-endian implementation for digest.c
#endif
    }

    return rem_poly ^ 0xffffffff;
}


digest_t _calculate_checksum(const void *buffer, size_t size)
{
    return crc32(buffer, size);
}


bool _check_integrity(const void *buffer, size_t size)
{
    return crc32(buffer, size) == 0x2144df1c;
}
