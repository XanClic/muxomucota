#ifndef _KSYM_H
#define _KSYM_h

#include <stdbool.h>
#include <stdint.h>


bool kernel_find_function(uintptr_t addr, char *name, uintptr_t *func_base);

#endif
