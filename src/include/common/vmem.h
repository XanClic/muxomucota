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

#ifndef _VMEM_H
#define _VMEM_H

#include <digest.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include <arch-vmem.h>


// Unprivilegiert
#define VMM_UR (1 << 0)
#define VMM_UW (1 << 1)
#define VMM_UX (1 << 2)
// Privilegiert
#define VMM_PR (1 << 3)
#define VMM_PW (1 << 4)
#define VMM_PX (1 << 5)

// cache disable
#define VMM_CD (1 << 6)


typedef struct
{
    struct arch_vmm_context_info arch;

    void *heap_end;

    int users;
} vmm_context_t;


extern vmm_context_t kernel_context;


struct shm_sg
{
    size_t size;
    int users;
    ptrdiff_t offset;
    bool kept_alive;

    DIGESTIFY

    uintptr_t phys[];
};


void init_virtual_memory(void);

void arch_init_virtual_memory(vmm_context_t *primordial_context);

void change_vmm_context(const vmm_context_t *context);
void create_vmm_context(vmm_context_t *context);
void create_vmm_context_arch(vmm_context_t *context);
void destroy_vmm_context(vmm_context_t *context);

void use_vmm_context(vmm_context_t *context);
void unuse_vmm_context(vmm_context_t *context);
void vmmc_clear_user(vmm_context_t *context, bool preserve_heritage);

void vmmc_set_heap_top(vmm_context_t *context, void *address);

void *kernel_map(uintptr_t phys, size_t length);
// non-consecutive mapping
void *kernel_map_nc(uintptr_t *pageframe_list, int frames);
uintptr_t kernel_unmap(void *virt, size_t length);

void vmmc_map_user_page_unlocked(const vmm_context_t *context, void *virt, uintptr_t phys, unsigned flags);
void vmmc_map_user_page(vmm_context_t *context, void *virt, uintptr_t phys, unsigned flags);
void vmmc_lazy_map_area(vmm_context_t *context, void *virt, ptrdiff_t sz, unsigned flags);

void *vmmc_user_map(vmm_context_t *context, uintptr_t phys, size_t length, unsigned flags);
void *vmmc_user_map_sg(vmm_context_t *context, uintptr_t *phys, int pages, unsigned flags);

void vmmc_user_unmap(vmm_context_t *context, void *virt, size_t length);

unsigned vmmc_address_mapped(vmm_context_t *context, void *virt, uintptr_t *phys);

void vmmc_clone(vmm_context_t *dest, vmm_context_t *source);

bool vmmc_do_cow(vmm_context_t *context, void *address);
bool vmmc_do_lazy_map(vmm_context_t *context, void *address);

uintptr_t vmmc_create_shm(size_t sz);
uintptr_t vmmc_make_shm(vmm_context_t *context, int count, void **vaddr_list, int *page_count_list, ptrdiff_t offset);
void *vmmc_open_shm(vmm_context_t *context, uintptr_t shm_id, unsigned flags);
void vmmc_close_shm(vmm_context_t *context, uintptr_t shm_id, void *virt, bool destroy);
void vmmc_unmake_shm(uintptr_t shm_id, bool destroy);
size_t vmmc_get_shm_size(uintptr_t shm_id);

bool is_valid_kernel_mem(const void *start, size_t length, unsigned flags);
bool is_valid_user_mem(vmm_context_t *context, const void *start, size_t length, unsigned flags);

bool is_valid_shm(uintptr_t shmid);


void *context_sbrk(vmm_context_t *context, ptrdiff_t inc);

#endif
