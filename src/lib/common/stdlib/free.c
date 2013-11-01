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

#include <assert.h>
#include <compiler.h>
#include <lock.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <syscall.h>


struct free_block
{
    struct free_block *next;
    size_t size;
};

extern volatile lock_t __heap_lock;

extern struct free_block *__first_free;


static void join_memory(void)
{
    struct free_block *current = __first_free;

    if (unlikely(current == NULL))
        return;

    while (current->next != NULL)
    {
        if ((uintptr_t)current + current->size + 16 > (uintptr_t)current->next)
            *(volatile void **)NULL = current; // no u

        if ((uintptr_t)current + current->size + 16 == (uintptr_t)current->next)
        {
            current->size += current->next->size + 16;
            current->next = current->next->next;
        }
        else
            current = current->next;
    }
}


void free(void *ptr)
{
    if (ptr == NULL)
        return;

    struct free_block *t = (struct free_block *)((uintptr_t)ptr - 16);

    lock(&__heap_lock);

    struct free_block **fb = &__first_free;

    while (((uintptr_t)*fb < (uintptr_t)t) && *fb)
        fb = &(*fb)->next;

#ifndef NDEBUG
    // Assert tut dann lustigerweise nicht mehr.
    if (*fb == t)
        *(volatile void **)NULL = ptr;
#endif

    t->next = *fb;
    *fb = t;

    join_memory();

    unlock(&__heap_lock);

    // TODO: sbrk(-x), wenn möglich
}
