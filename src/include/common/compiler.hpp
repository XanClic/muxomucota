#ifndef _COMPILER_HPP
#define _COMPILER_HPP

#ifdef __GNUC__
#define cxx_packed       __attribute__((packed))
#define cxx_noreturn     __attribute__((noreturn))
#define cxx_weak         __attribute__((weak))
#define cxx_pure         __attribute__((pure))
#define cxx_unreachable  __builtin_unreachable()
#define cxx_unused(func) func __attribute__((unused))
#define likely(x)       __builtin_expect((bool)(x), true)
#define unlikely(x)     __builtin_expect((bool)(x), false)
#else
#error Unknown compiler.
#endif

#endif
