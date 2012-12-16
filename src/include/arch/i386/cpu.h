#ifndef _CPU_H
#define _CPU_H

static inline void cpu_halt(void) { __asm__ __volatile__ ("hlt"); }

#endif
