#include <arch-constants.h>
#include <cpu.h>
#include <ksym.h>
#include <panic.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <vmem.h>


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


    // FIXME: Das ist architekturspezifisch.
    uint32_t *ebp;
    __asm__ __volatile__ ("mov %0,ebp" : "=r"(ebp));

    y += 2;


    uintptr_t phys_ebp;
    while ((ebp != NULL) && (current_process != NULL) && vmmc_address_mapped(current_process->vmmc, ebp, &phys_ebp))
    {
        dst = (int8_t *)((0xb8000 | PHYS_BASE) + y++ * 160);

        uint32_t pre = ebp[0];

        uint32_t func = ebp[1];
        for (int i = 0; i < 8; i++)
        {
            int d = (func >> ((7 - i) * 4)) & 0xf;
            *(dst++) = (d > 9) ? (d - 10 + 'a') : (d + '0');
            *(dst++) = 0x4f;
        }


        ebp = (uint32_t *)pre;


        char name[60];
        uintptr_t func_base;

        if (!kernel_find_function(func, name, &func_base))
            continue;


        *(dst++) = ' ';
        *(dst++) = 0x4f;
        *(dst++) = '(';
        *(dst++) = 0x4f;

        for (int i = 0; name[i]; i++)
        {
            *(dst++) = name[i];
            *(dst++) = 0x4f;
        }

        *(dst++) = '+';
        *(dst++) = 0x4f;

        bool got = false;
        ptrdiff_t diff = func - func_base;
        for (int i = 0; i < 8; i++)
        {
            int d = (diff >> ((7 - i) * 4)) & 0xf;
            if (d || (i == 7))
                got = true;
            if (got)
            {
                *(dst++) = (d > 9) ? (d - 10 + 'a') : (d + '0');
                *(dst++) = 0x4f;
            }
        }

        *(dst++) = ')';
        *(dst++) = 0x4f;
    }


    __asm__ __volatile__ ("cli");

    for (;;)
        cpu_halt();
}
