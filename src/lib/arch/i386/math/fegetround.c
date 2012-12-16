#include <fenv.h>
#include <stdint.h>

int fegetround(void)
{
    uint16_t ctrl_word;

    __asm__ __volatile__ ("fstcw %0" : "=m"(ctrl_word));

    return (ctrl_word >> 10) & 3;
}
