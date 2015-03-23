#include <cstdint>
#include <kernel_object.hpp>


static uint32_t crc32(const void *buffer, size_t size)
{
    uint32_t rem_poly = UINT32_C(0xffffffff);
    const uint8_t *bp = static_cast<const uint8_t *>(buffer);

    while (size--) {
#ifdef LITTLE_ENDIAN
        rem_poly ^= *(bp++);

        for (int i = 0; i < 8; i++) {
            if (rem_poly & 1) {
                rem_poly = (rem_poly >> 1) ^ UINT32_C(0xedb88320);
            } else {
                rem_poly >>= 1;
            }
        }
#else
#error Missing non-little-endian implementation for crc32()
#endif
    }

    return rem_poly ^ UINT32_C(0xffffffff);
}


kernel_object::kernel_object(size_t sz):
    size(sz)
{
}


bool kernel_object::check_integrity(void) const
{
    return crc32(this, size);
}


void kernel_object::refresh_checksum(void)
{
    digest = 0;
    digest = crc32(this, size);
}
