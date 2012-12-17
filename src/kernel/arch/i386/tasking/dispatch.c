#include <process.h>
#include <x86-segments.h>


extern struct tss x86_tss;


void arch_process_change(process_t *new_process)
{
    x86_tss.esp0 = new_process->arch.kernel_stack_top;
}


void yield(void)
{
    // FIXME.
    __asm__ __volatile__ ("int 0x20");
}
