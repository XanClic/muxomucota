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

#ifndef _SHM_H
#define _SHM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vmem.h>


uintptr_t shm_create(size_t size);
uintptr_t shm_make(int count, void **vaddr_list, int *page_count_list, ptrdiff_t offset);
void *shm_open(uintptr_t shmid, unsigned flags);
void shm_close(uintptr_t shmid, const void *ptr, bool destroy);
void shm_unmake(uintptr_t shmid, bool destroy);

size_t shm_size(uintptr_t shmid);

#endif
