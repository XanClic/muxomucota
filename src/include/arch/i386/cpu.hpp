#ifndef _CPU_HPP
#define _CPU_HPP

static inline void cpu_halt(void) { __asm__ __volatile__ ("hlt"); }

#endif
