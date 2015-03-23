#ifndef _ARCH_SYSCALL_HPP
#define _ARCH_SYSCALL_HPP

#include <compiler.hpp>
#include <cstdint>


cxx_unused(static uintptr_t syscall(int snr));
cxx_unused(static uintptr_t syscall(int snr, uintptr_t p0));
cxx_unused(static uintptr_t syscall(int snr, uintptr_t p0, uintptr_t p1));
cxx_unused(static uintptr_t syscall(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2));
cxx_unused(static uintptr_t syscall(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3));
cxx_unused(static uintptr_t syscall(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4));

static uintptr_t syscall(int snr)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xa2" : "=a"(ret) : "a"(snr) : "memory");
    return ret;
}

static uintptr_t syscall(int snr, uintptr_t p0)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xa2" : "=a"(ret) : "a"(snr), "b"(p0) : "memory");
    return ret;
}

static uintptr_t syscall(int snr, uintptr_t p0, uintptr_t p1)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xa2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1) : "memory");
    return ret;
}

static uintptr_t syscall(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xa2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1), "d"(p2) : "memory");
    return ret;
}

static uintptr_t syscall(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xa2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1), "d"(p2), "S"(p3) : "memory");
    return ret;
}

static uintptr_t syscall(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xa2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1), "d"(p2), "S"(p3), "D"(p4) : "memory");
    return ret;
}

#endif
