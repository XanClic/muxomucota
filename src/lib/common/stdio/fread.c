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

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <vfs.h>

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t addend = 0;

    if (stream->bufsz)
    {
        char *cbuf = ptr;

        while ((stream->bufsz >= size) && nmemb)
        {
            for (unsigned i = 0; i < size; i++)
                *(cbuf++) = stream->buffer[--stream->bufsz];
            nmemb--;
            addend++;
        }

        if (!nmemb)
            return addend;

        // TODO: Was, wenn stream->bufsz != 0?
    }

    size_t got = stream_recv(filepipe(stream), ptr, size * nmemb, 0) / size;
    if (got + addend < nmemb)
        stream->eof = true;
    return got + addend;
}
