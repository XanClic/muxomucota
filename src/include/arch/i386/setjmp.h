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

#ifndef _SETJMP_H
#define _SETJMP_H

#include <compiler.h>
#include <stdint.h>
#include <stdnoreturn.h>


typedef struct
{
    uint32_t esp, eip;
    uint32_t ebx, esi, edi, ebp;
} cc_packed jmp_buf;


int setjmp(jmp_buf env);
noreturn void longjmp(jmp_buf env, int val);

#define setjmp(buf) _setjmp(&(buf))
#define longjmp(buf, val) _longjmp(&(buf), val)

int _setjmp(jmp_buf *env);
noreturn void _longjmp(jmp_buf *env, int val);

#endif
