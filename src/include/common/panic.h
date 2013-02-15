#ifndef _PANIC_H
#define _PANIC_H

#include <stdnoreturn.h>


noreturn void panic(const char *msg);
noreturn void format_panic(const char *format, ...);

#endif
