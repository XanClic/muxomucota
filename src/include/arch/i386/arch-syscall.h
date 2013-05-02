/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

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
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr) : "memory");
    return ret;
}

static uintptr_t syscall1(int snr, uintptr_t p0)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0) : "memory");
    return ret;
}

static uintptr_t syscall2(int snr, uintptr_t p0, uintptr_t p1)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1) : "memory");
    return ret;
}

static uintptr_t syscall3(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1), "d"(p2) : "memory");
    return ret;
}

static uintptr_t syscall4(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1), "d"(p2), "S"(p3) : "memory");
    return ret;
}

static uintptr_t syscall5(int snr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    uintptr_t ret;
    __asm__ __volatile__ ("int $0xA2" : "=a"(ret) : "a"(snr), "b"(p0), "c"(p1), "d"(p2), "S"(p3), "D"(p4) : "memory");
    return ret;
}

#endif
