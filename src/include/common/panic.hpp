#ifndef _PANIC_HPP
#define _PANIC_HPP

#include <compiler.hpp>


void panic(const char *msg) cxx_noreturn;
void format_panic(const char *format, ...) cxx_noreturn;

#endif
