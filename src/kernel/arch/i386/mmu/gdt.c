// Minimale GDT-Implementierung.

#include <compiler.h>
#include <stdint.h>
#include <vmem.h>
#include <x86-segments.h>

#include <arch-vmem.h>


struct tss x86_tss = {
    .ss0 = SEG_SYS_DS
};


static uint64_t gdt[6];

void init_gdt(void)
{
    gdt[1] = UINT64_C(0x00CF9A000000FFFF); // Kernelcodesegment
    gdt[2] = UINT64_C(0x00CF92000000FFFF); // Kerneldatensegment
    gdt[3] = UINT64_C(0x00CEFA000000FFFF); // Usercodesegment
    gdt[4] = UINT64_C(0x00CEF2000000FFFF); // Userdatensegment
    gdt[5] = UINT64_C(0xF040E90000000000) | (sizeof(x86_tss) - 1); // TSS

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
                          "mov fs,ax;"
                          "mov gs,ax;"
                          "mov ss,ax;"
                          :: "a"(SEG_SYS_DS), "b"(&gdtr) : "memory");

    __asm__ __volatile__ ("ltr %0" :: "r"((uint16_t)0x28));
}
