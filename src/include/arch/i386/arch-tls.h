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

#ifndef _ARCH_TLS_H
#define _ARCH_TLS_H

#include <compiler.h>


#ifndef KERNEL
static inline struct tls *__tls(void) cc_pure;

static inline struct tls *__tls(void)
{
    struct tls *tls;
    __asm__ __volatile__ ("mov %%fs:(0),%0" : "=r"(tls));
    return tls;
}
#endif

#endif
