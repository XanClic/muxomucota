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

#ifndef _DIGEST_H
#define _DIGEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef uint32_t digest_t;


digest_t _calculate_checksum(const void *buffer, size_t size);
bool _check_integrity(const void *buffer, size_t size);


// Beide Makros erfordern die Existenz eines „digest_t digest“-Felds in der
// Struktur; die Integrität wird nur bis zu diesem Feld gewährleistet.
#define calculate_checksum(structure) _calculate_checksum(&(structure), (uintptr_t)&((structure).digest) - (uintptr_t)&(structure))
#define check_integrity(structure) _check_integrity(&(structure), (uintptr_t)&((structure).digest) + sizeof(digest_t) - (uintptr_t)&(structure))


// Fügt besagtes „digest_t digest“-Feld in eine Struktur ein
#define DIGESTIFY digest_t digest;

#endif
