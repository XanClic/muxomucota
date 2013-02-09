#include <arch-constants.h>
#include <cpu.h>
#include <panic.h>
#include <stdint.h>
#include <stdnoreturn.h>


noreturn void panic(const char *msg)
{
    int8_t *dst = (int8_t *)(0xb8000 | PHYS_BASE);
    int x = 0, y = 0;

    while (*msg)
    {
        switch (*msg)
        {
            case '\n':
                y++;
            case '\r':
                x = 0;
                dst = (int8_t *)((0xb8000 | PHYS_BASE) + y * 160);
                msg++;
                break;
            default:
                *(dst++) = *(msg++);
                *(dst++) = 0x4f;
                if (++x >= 80)
                {
                    x -= 80;
                    y++;
                }
        }
    }


    __asm__ __volatile__ ("cli");

    for (;;)
        cpu_halt();
}
