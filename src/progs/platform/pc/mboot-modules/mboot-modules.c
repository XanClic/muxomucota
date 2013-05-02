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

#include <ctype.h>
#include <drivers.h>
#include <drivers/memory.h>
#include <errno.h>
#include <multiboot.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


struct module
{
    struct module *next;

    char *name;

    void *ptr;
    size_t size;
};


struct module_handle
{
    struct module *mod;

    off_t file_off;
};


static int module_count;
static struct module *first_module;


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)flags;

    struct module *mod;

    for (mod = first_module; (mod != NULL) && strcmp(mod->name, relpath); mod = mod->next);

    if (mod == NULL)
    {
        errno = ENOENT;
        return 0;
    }

    struct module_handle *hmod = malloc(sizeof(*hmod));
    hmod->mod = mod;
    hmod->file_off = 0;

    return (uintptr_t)hmod;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    free((void *)id);
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct module_handle *hmod = (struct module_handle *)id;
    struct module_handle *new_hmod = malloc(sizeof(*new_hmod));

    return (uintptr_t)memcpy(new_hmod, hmod, sizeof(*hmod));
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;


    struct module_handle *hmod = (struct module_handle *)id;


    size_t copy_sz = ((int)size <= hmod->mod->size - hmod->file_off) ? size : (size_t)(hmod->mod->size - hmod->file_off);

    memcpy(data, (uint8_t *)hmod->mod->ptr + (ptrdiff_t)hmod->file_off, copy_sz);

    hmod->file_off += copy_sz;


    return copy_sz;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct module_handle *hmod = (struct module_handle *)id;


    switch (flag)
    {
        case F_PRESSURE:
            return hmod->mod->size - hmod->file_off;
        case F_POSITION:
            return hmod->file_off;
        case F_FILESIZE:
            return hmod->mod->size;
        case F_READABLE:
            return (hmod->mod->size - hmod->file_off) > 0;
        case F_WRITABLE:
            return false;
    }


    errno = EINVAL;
    return 0;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    struct module_handle *hmod = (struct module_handle *)id;


    switch (flag)
    {
        case F_POSITION:
            hmod->file_off = (value > hmod->mod->size) ? hmod->mod->size : value;
            return 0;
        case F_FLUSH:
            return 0;
    }


    errno = EINVAL;
    return -EINVAL;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return ARRAY_CONTAINS((int[]){ I_FILE }, interface);
}


int main(int argc, char *argv[])
{
    uintptr_t mbi_paddr = 0;

    for (int i = 0; i < argc; i++)
    {
        if (!strncmp(argv[i], "mbi=", 4))
        {
            for (int j = 0; j < 8; j++)
            {
                int hex_digit = argv[i][j + 4];

                hex_digit = isdigit(hex_digit) ? (hex_digit - '0') : (tolower(hex_digit) - 'a' + 10);

                mbi_paddr |= hex_digit << ((7 - j) * 4);
            }
        }
    }


    if (!mbi_paddr)
        return 1;


    struct multiboot_info *mbi = map_memory(mbi_paddr, sizeof(*mbi), VMM_UR);

    module_count = mbi->mods_count;
    struct multiboot_module *modules = map_memory(mbi->mods_addr, sizeof(modules[0]) * module_count, VMM_UR);

    unmap_memory(mbi, sizeof(*mbi));


    struct module **nxtptr = &first_module;

    for (int i = 0; i < module_count; i++)
    {
        *nxtptr = malloc(sizeof(**nxtptr));


        const char *src = map_memory(modules[i].string, 1024, VMM_UR); // 1k ought to be enough

        int j;
        for (j = 0; src[j] && (src[j] != ' ') && (j < 1024); j++);

        (*nxtptr)->name = malloc(j + 1);

        memcpy((*nxtptr)->name, src, j);
        (*nxtptr)->name[j] = 0;

        unmap_memory(src, 1024);


        (*nxtptr)->size = modules[i].mod_end - modules[i].mod_start;

        (*nxtptr)->ptr = malloc((*nxtptr)->size);

        const void *data = map_memory(modules[i].mod_start, (*nxtptr)->size, VMM_UR);
        memcpy((*nxtptr)->ptr, data, (*nxtptr)->size);
        unmap_memory(data, (*nxtptr)->size);


        nxtptr = &(*nxtptr)->next;
    }


    *nxtptr = NULL;


    daemonize("mboot", true);
}
