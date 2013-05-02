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

#ifndef _PMM_H
#define _PMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


void init_pmm(void);

uintptr_t pmm_alloc(void);
uintptr_t pmm_alloc_advanced(size_t length, uintptr_t max_addr, int alignment);
void pmm_free(uintptr_t paddr);

void pmm_use(uintptr_t paddr);

int pmm_user_count(uintptr_t paddr);
bool pmm_alloced(uintptr_t paddr);

void pmm_mark_cow(uintptr_t paddr, bool flag);
bool pmm_is_cow(uintptr_t paddr);

#endif
