#ifndef _ARCH_SYSCALL_H
#define _ARCH_SYSCALL_H

#include <compiler.h>
#include <stdint.h>


cc_unused(static uintptr_t syscall0(int snr));
cc_unused(static uintptr_t syscall1(int snr, uintptr_t p0));
cc_unused(static uintptr_t syscall2(int snr, uintptr_t p0, uintptr_t p1));
cc_unused(static uintptr_t syscall3(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2));
cc_unused(static uintptr_t syscall4(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3));
cc_unused(static uintptr_t syscall5(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4));

static uintptr_t syscall0(int snr)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr));
    return ret;
}

static uintptr_t syscall1(int snr, uintptr_t p0)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0));
    return ret;
}

static uintptr_t syscall2(int snr, uintptr_t p0, uintptr_t p1)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1));
    return ret;
}

static uintptr_t syscall3(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1), "d"(p2));
    return ret;
}

static uintptr_t syscall4(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1), "d"(p2), "S"(p3));
    return ret;
}

static uintptr_t syscall5(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1), "d"(p2), "S"(p3), "D"(p4));
    return ret;
}

#endif
