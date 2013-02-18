/************************************************************************
 * Copyright (C) 2009, 2010 by Hanna Reitz                              *
 * xanclic@googlemail.com                                               *
 *                                                                      *
 * This program is free software; you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation;  either version 2 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program  is  distributed  in the hope  that it will  be useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied  warranty of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the  GNU  General  Public License *
 * along with this program; if not, write to the                        *
 * Free Software Foundation, Inc.,                                      *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.            *
 ************************************************************************/

#include <lock.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <vfs.h>

static volatile lock_t printf_lock = LOCK_INITIALIZER;
static char printf_buffer[4096];

int printf(const char *format, ...)
{
    va_list args;
    int ret;

    lock(&printf_lock);

    va_start(args, format);
    ret = vsnprintf(printf_buffer, 4096, format, args);
    va_end(args);

    stream_send(STDOUT_FILENO, printf_buffer, (ret > 4096) ? 4096 : ret, 0);

    unlock(&printf_lock);

    return ret;
}
