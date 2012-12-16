#include <errno.h>
#include <fenv.h>
#include <stdint.h>

int fesetround(int mode)
{
    if (mode & ~3)
    {
        errno = EINVAL;
        return -1;
    }

    uint16_t ctrl_word;

    __asm__ __volatile__ ("fstcw %0" : "=m"(ctrl_word));

    ctrl_word = (ctrl_word & ~(3 << 10)) | (mode << 10);

    __asm__ __volatile__ ("fldcw %0" :: "m"(ctrl_word));

    return 0;
}
