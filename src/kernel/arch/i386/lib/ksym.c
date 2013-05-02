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

#include <arch-constants.h>
#include <elf.h>
#include <ksym.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vmem.h>


extern Elf32_Shdr *kernel_elf_shdr;
extern int kernel_elf_shdr_count;


bool kernel_find_function(uintptr_t addr, char *name, uintptr_t *func_base)
{
    Elf32_Sym *elf_sym = NULL;
    char *strtbl = NULL;
    int syms = 0;


    for (int i = 0; i < kernel_elf_shdr_count; i++)
    {
        if (kernel_elf_shdr[i].sh_type == SH_SYMTAB)
        {
            // FIXME: Eigentlich müsste man kernel_map benutzen, aber das ist im
            // Falle einer schweren Exception vielleicht doof (es könnte bspw.
            // passieren, dass kernel_map den PMM benutzt, etc.)
            elf_sym = (void *)(kernel_elf_shdr[i].sh_addr | PHYS_BASE);
            syms = kernel_elf_shdr[i].sh_size / sizeof(Elf32_Sym);

            strtbl = (void *)(kernel_elf_shdr[kernel_elf_shdr[i].sh_link].sh_addr | PHYS_BASE);

            break;
        }
    }


    if (elf_sym == NULL)
        return false;


    uintptr_t best_hit = 0;
    int best_hit_idx = -1;

    for (int i = 0; i < syms; i++)
    {
        if (((ELF32_ST_TYPE(elf_sym[i].st_info) == STT_FUNC) || (ELF32_ST_TYPE(elf_sym[i].st_info) == STT_OBJECT)) &&
             (elf_sym[i].st_value <= addr) && (elf_sym[i].st_value > best_hit))
        {
            best_hit = elf_sym[i].st_value;
            best_hit_idx = i;
        }
    }


    if (best_hit_idx < 0)
        return false;


    *func_base = best_hit;
    strcpy(name, &strtbl[elf_sym[best_hit_idx].st_name]);


    return true;
}
