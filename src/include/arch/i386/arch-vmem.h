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

#ifndef _ARCH_VMEM_H
#define _ARCH_VMEM_H

#include <lock.h>
#include <stdint.h>


#define MAP_PR (1U << 0)
#define MAP_NP (!MAP_PR)
#define MAP_RW (1U << 1)
#define MAP_RO (!MAP_RW)
#define MAP_US (1U << 2)
#define MAP_OS (!MAP_US)
#define MAP_WT (1U << 3)
#define MAP_NC (1U << 4)
#define MAP_CC (!MAP_NC)
#define MAP_4M (1U << 7)
#define MAP_4K (!MAP_4M)
#define MAP_GB (1U << 8)
#define MAP_LC (!MAP_GB)

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)


#define IS_ALIGNED(addr) (((uintptr_t)(addr) & (PAGE_SIZE - 1)) == 0)


struct arch_vmm_context_info
{
    uintptr_t cr3;
    uint32_t *pd;

    lock_t lock;
};


void init_gdt(void);

#endif
