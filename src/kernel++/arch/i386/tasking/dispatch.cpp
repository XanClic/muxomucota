#include <process.hpp>
#include <x86-segments.hpp>


extern tss x86_tss;


void process::arch_switch_to(void)
{
    x86_tss.esp0 = arch.kernel_stack_top;

    tls->use();

    __asm__ __volatile__ ("mov eax,cr0; or eax,8; mov cr0,eax" ::: "eax"); // TS setzen
}
