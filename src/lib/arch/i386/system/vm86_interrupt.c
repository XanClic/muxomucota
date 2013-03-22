#include <stdint.h>
#include <syscall.h>
#include <vm86.h>


void vm86_interrupt(int intr, struct vm86_registers *regs, struct vm86_memory_area *mem, int areas)
{
    syscall4(SYS_VM86, intr, (uintptr_t)regs, (uintptr_t)mem, areas);
}
