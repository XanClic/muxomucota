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

#include <drivers.h>
#include <errno.h>
#include <lock.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


static struct entry
{
    struct entry *next;
    char *base, *dest;
} *entries = NULL;

static rw_lock_t entry_lock = RW_LOCK_INITIALIZER;


struct mount_point_in_creation
{
    char *path;
};


uintptr_t service_create_pipe(const char *path, int flags)
{
    if (!(flags & O_CREAT_MOUNT_POINT))
    {
        errno = ENOENT;
        return 0;
    }

    struct mount_point_in_creation *mpic = malloc(sizeof(*mpic));
    mpic->path = strdup(path);

    return (uintptr_t)mpic;
}


bool service_is_symlink(const char *relpath, char *linkpath)
{
    struct entry *best_match = NULL;
    int best_match_len = -1;

    rwl_lock_r(&entry_lock);

    for (struct entry *e = entries; e != NULL; e = e->next)
    {
        int len = strlen(e->base);

        if ((len > best_match_len) && !strncmp(relpath, e->base, len) && (!relpath[len] || (relpath[len] == '/')))
        {
            best_match = e;
            best_match_len = len;
        }
    }

    if (best_match == NULL)
    {
        rwl_unlock_r(&entry_lock);
        return false;
    }

    strcpy(linkpath, best_match->dest);
    strcat(linkpath, relpath + best_match_len);

    rwl_unlock_r(&entry_lock);

    return true;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    free((void *)id);
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    // Erscheint mir zwar suspekt, aber gut.

    struct mount_point_in_creation *mpic = (struct mount_point_in_creation *)id, *nmpic = malloc(sizeof(*nmpic));
    nmpic->path = strdup(mpic->path);

    return (uintptr_t)nmpic;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return interface == I_FS;
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    if (flags != F_MOUNT_FILE)
    {
        errno = EINVAL;
        return 0;
    }


    const char *path = data;

    if (!size || path[size - 1])
    {
        errno = EINVAL;
        return 0;
    }


    struct mount_point_in_creation *mpic = (struct mount_point_in_creation *)id;


    // Ob der Pfad zu öffnen ist, ist dem Mapper hier ja völlig egal.

    rwl_lock_w(&entry_lock);

    struct entry **ep;
    for (ep = &entries; (*ep != NULL) && strcmp((*ep)->base, mpic->path); ep = &(*ep)->next);

    if (*ep == NULL)
    {
        *ep = malloc(sizeof(**ep));
        (*ep)->next = NULL;
        (*ep)->base = strdup(mpic->path);
        (*ep)->dest = NULL;
    }

    free((*ep)->dest);
    (*ep)->dest = strdup(path);

    rwl_unlock_w(&entry_lock);


    return size;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    (void)id;

    switch (flag)
    {
        case F_PRESSURE:
            return 0;
        case F_READABLE:
        case F_WRITABLE:
            return false;
    }

    errno = EINVAL;
    return 0;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    (void)id;
    (void)value;

    switch (flag)
    {
        case F_FLUSH:
            return 0;
    }

    errno = EINVAL;
    return -EINVAL;
}


int main(void)
{
    daemonize("root", true);
}
