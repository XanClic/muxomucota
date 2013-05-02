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

#ifndef _LOCK_H
#define _LOCK_H

#include <stdbool.h>
#include <sys/types.h>


#ifdef KERNEL

typedef pid_t lock_t;


#define LOCK_INITIALIZER 0
#define IS_UNLOCKED(l)   ((l) == 0)
#define IS_LOCKED(l)     ((l) != 0)


static inline void lock_init(lock_t *lock) { *lock = 0; }

#else

typedef pid_t lock_t;


#define LOCK_INITIALIZER 0
#define IS_UNLOCKED(l)   ((l) <= 0)
#define IS_LOCKED(l)     ((l) >  0)


static inline void lock_init(lock_t *lock) { *lock = 0; }

#endif


void lock(volatile lock_t *v);
void unlock(volatile lock_t *v);
bool trylock(volatile lock_t *v);


#ifndef KERNEL

// readers-writers
typedef struct
{
    lock_t write_lock, lock_lock;
    int reader_count;
} rw_lock_t;

#define RW_LOCK_INITIALIZER { .write_lock = LOCK_INITIALIZER, .reader_count = 0 }

static inline void rwl_lock_init(rw_lock_t *rwl) { lock_init(&rwl->write_lock); lock_init(&rwl->lock_lock); rwl->reader_count = 0; }

void rwl_lock_r(volatile rw_lock_t *rwl);
void rwl_unlock_r(volatile rw_lock_t *rwl);
void rwl_lock_w(volatile rw_lock_t *rwl);
void rwl_unlock_w(volatile rw_lock_t *rwl);
void rwl_require_w(volatile rw_lock_t *rwl);
void rwl_drop_w(volatile rw_lock_t *rwl);

#endif

#endif
