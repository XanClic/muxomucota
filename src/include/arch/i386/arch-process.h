#ifndef _ARCH_PROCESS_H
#define _ARCH_PROCESS_H

#include <stdint.h>


struct process_arch_info
{
    uintptr_t kernel_stack_top;
};

#endif
