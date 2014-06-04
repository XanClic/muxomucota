#ifndef _KSYM_HPP
#define _KSYM_HPP

#include <cstdint>


bool kernel_find_function(uintptr_t addr, char *name, uintptr_t *func_base);

#endif
