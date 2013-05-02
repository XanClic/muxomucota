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

#ifndef _ARCH_CONSTANTS_H
#define _ARCH_CONSTANTS_H

#define KERNEL_BASE 0xF0000000U

#define PHYS_BASE 0xF0000000U
#define HMEM_BASE 0xF0400000U
#define UNMP_BASE 0xFFC00000U

#define USER_HERITAGE_BASE 0xBF000000U
#define USER_HERITAGE_TOP  0xC0000000U
#define USER_MAP_BASE      0xC0000000U
#define USER_MAP_TOP       0xE0000000U
#define USER_STACK_BASE    0xE0000000U
#define USER_STACK_TOP     0xF0000000U

#define IRQ_COUNT 16

#define IS_KERNEL(x) ((uintptr_t)(x) >= PHYS_BASE)

#endif
