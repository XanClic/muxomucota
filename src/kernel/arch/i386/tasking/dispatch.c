#include <cpu-state.h>
#include <ipc.h>
#include <process.h>
#include <tls.h>
#include <x86-segments.h>


extern struct tss x86_tss;


void arch_process_change(process_t *new_process)
{
    x86_tss.esp0 = new_process->arch.kernel_stack_top;

    set_tls(new_process->tls);
}
