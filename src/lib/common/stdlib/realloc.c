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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>


struct free_block
{
    struct free_block *next;
    size_t size;
};


void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return malloc(size);

    if (!size)
    {
        free(ptr);
        return NULL;
    }

    size_t osize = ((struct free_block *)((uintptr_t)ptr - 16))->size;

    if (osize >= size)
        return ptr;

    void *nmem = malloc(size);
    if (nmem == NULL)
        return NULL;

    memcpy(nmem, ptr, osize);
    free(ptr);

    return nmem;
}
