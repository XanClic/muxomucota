#ifndef _ARCH_CONSTANTS_HPP
#define _ARCH_CONSTANTS_HPP

#include <cstdint>


#define KERNEL_BASE 0xf0000000u

#define PHYS_BASE 0xf0000000u
#define HMEM_BASE 0xf0400000u
#define UNMP_BASE 0xffc00000u

#define USER_HERITAGE_BASE 0xbf000000u
#define USER_HERITAGE_TOP  0xc0000000u
#define USER_MAP_BASE      0xc0000000u
#define USER_MAP_TOP       0xe0000000u
#define USER_STACK_BASE    0xe0000000u
#define USER_STACK_TOP     0xf0000000u

#define IRQ_COUNT 16

#define IS_KERNEL(x) (reinterpret_cast<uintptr_t>(x) >= PHYS_BASE)

#endif
