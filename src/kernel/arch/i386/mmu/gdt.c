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

// Minimale GDT-Implementierung.

#include <compiler.h>
#include <stdint.h>
#include <tls.h>
#include <vmem.h>
#include <x86-segments.h>

#include <arch-vmem.h>


struct tss x86_tss = {
    .ss0 = SEG_SYS_DS
};


static uint64_t gdt[7];

void init_gdt(void)
{
    gdt[1] = UINT64_C(0x00cf9a000000ffff); // Kernelcodesegment
    gdt[2] = UINT64_C(0x00cf92000000ffff); // Kerneldatensegment
    gdt[3] = UINT64_C(0x00cefa000000ffff); // Usercodesegment
    gdt[4] = UINT64_C(0x00cef2000000ffff); // Userdatensegment
    gdt[5] = UINT64_C(0x0040e90000000000) | (sizeof(x86_tss) - 1); // TSS
    gdt[6] = UINT64_C(0x0040f20000000000); // TLS

    uint64_t tss_base = (uintptr_t)&x86_tss;
    gdt[5] |= (tss_base & 0x00FFFFFF) << 16;
    gdt[5] |= (tss_base & 0xFF000000) << 32;

    static struct
    {
        uint16_t limit;
        uintptr_t base;
    } cc_packed gdtr = {
        .limit = sizeof(gdt) - 1,
        .base = (uintptr_t)gdt
    };

    __asm__ __volatile__ ("" ::: "memory");

    __asm__ __volatile__ ("lgdt [%1];"
                          ".byte 0xE8;" // call +0
                          ".int 0;"
                          "pop ebx;" // EIP
                          "add ebx,10;"
                          "push 0x08;"
                          "push ebx;"
                          "retf;"
                          "mov ds,ax;"
                          "mov es,ax;"
                          "mov ss,ax;"
                          :: "a"(SEG_SYS_DS), "b"(&gdtr) : "memory");

    __asm__ __volatile__ ("ltr %0" :: "r"((uint16_t)0x28));
}


void set_tls(struct tls *ptr)
{
    gdt[6] = UINT64_C(0x0040f20000000000) | sizeof(*ptr);

    gdt[6] |= ((uint64_t)(uintptr_t)ptr & 0x00ffffff) << 16;
    gdt[6] |= ((uint64_t)(uintptr_t)ptr & 0xff000000) << 32;
}
