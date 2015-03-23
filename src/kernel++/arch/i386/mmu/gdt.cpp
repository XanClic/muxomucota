#include <compiler.hpp>
#include <tls.hpp>
#include <vmem.hpp>

#include <x86-segments.hpp>


tss x86_tss;

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
    gdt[5] |= (tss_base & 0x00ffffff) << 16;
    gdt[5] |= (tss_base & 0xff000000) << 32;

    static struct {
        uint16_t limit;
        uintptr_t base;
    } cxx_packed gdtr = {
        sizeof(gdt) - 1,
        reinterpret_cast<uintptr_t>(gdt)
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

    __asm__ __volatile__ ("ltr %0" :: "r"(static_cast<uint16_t>(0x28)));
}


void tls::use(void)
{
    gdt[6] = UINT64_C(0x0040f20000000000) | sizeof(*this);

    gdt[6] |= (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)) & 0x00ffffff) << 16;
    gdt[6] |= (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)) & 0xff000000) << 32;
}
