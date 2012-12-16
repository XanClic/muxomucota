#ifndef _CPU_STATE_H
#define _CPU_STATE_H

#include <compiler.h>
#include <stdint.h>


struct cpu_state
{
    uint32_t ebp, edi, esi, edx, ecx, ebx, eax;
    uint32_t es, ds;
    uint32_t int_vector, err_code;
    uint32_t eip, cs, eflags, esp, ss;
} cc_packed;

#endif
