#include <kassert.h>
#include <lock.h>
#include <pmm.h>
#include <process.h>
#include <stdbool.h>
#include <string.h>
#include <vmem.h>

#include <arch-vmem.h>
#include <arch-constants.h>


extern const void hpstructs;

static uint32_t *kpd, *kpt;
static lock_t kpt_lock = LOCK_INITIALIZER;


void change_vmm_context(const vmm_context_t *ctx)
{
    __asm__ __volatile__ ("mov cr3,%0;" :: "r"(ctx->arch.cr3) : "memory");
}

static void invlpg(uintptr_t address)
{
    __asm__ __volatile__ ("invlpg [%0];" :: "r"(address) : "memory");
}

static void full_tlb_invalidation(void)
{
    __asm__ __volatile__ ("mov eax,cr3; mov cr3,eax;" ::: "eax", "memory");
}


void arch_init_virtual_memory(vmm_context_t *primctx)
{
    primctx->arch.cr3 = (uintptr_t)&hpstructs - PHYS_BASE;
    primctx->arch.pd = (uint32_t *)&hpstructs;


    kpd = primctx->arch.pd;
    kpt = kpd + 1024;


    unsigned i = 0;

    for (; i < (PHYS_BASE >> 22); i++)
        kpd[i] = MAP_NP;

    for (; i < (HMEM_BASE >> 22); i++)
        kpd[i] = (((uintptr_t)i << 22) - PHYS_BASE) | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC | MAP_4M;

    for (; i < (UNMP_BASE >> 22); i++)
        kpd[i] = ((uintptr_t)kpt - PHYS_BASE + (i - (HMEM_BASE >> 22)) * 4096) | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC | MAP_4K;

    kpd[i] = MAP_NP;


    for (i = 0; i < ((UNMP_BASE - HMEM_BASE) >> 12); i++)
        kpt[i] = MAP_NP;


    change_vmm_context(primctx);


    init_gdt();
}


void *kernel_map(uintptr_t phys, size_t length)
{
    if (phys + length <= HMEM_BASE - PHYS_BASE)
        return (void *)(phys | PHYS_BASE);


    uintptr_t off = phys & 0xFFF;

    phys &= ~0xFFF;


    int pages = (length + off + 4095) >> 12;


    // TODO: Irgendwie optimieren
    for (int i = 0; i < (int)((UNMP_BASE - HMEM_BASE) >> 12); i++)
    {
        if (kpt[i] == MAP_NP)
        {
            lock(&kpt_lock);

            int j;
            for (j = 0; j < pages; j++)
                if (kpt[i + j] != MAP_NP)
                    break;

            if (kpt[i + j] != MAP_NP)
            {
                unlock(&kpt_lock);
                i += j;
                continue;
            }

            for (j = 0; j < pages; j++)
                kpt[i + j] = (phys + j * 4096) | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC;

            unlock(&kpt_lock);

            return (void *)(HMEM_BASE + (uintptr_t)i * 4096 + off);
        }
    }


    return NULL;
}


void *kernel_map_nc(uintptr_t *pageframe_list, int frames)
{
    for (int i = 0; i < (int)((UNMP_BASE - HMEM_BASE) >> 12); i++)
    {
        if (kpt[i] == MAP_NP)
        {
            if (frames == 1)
            {
                if (!__sync_bool_compare_and_swap(&kpt[i], MAP_NP, pageframe_list[0] | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC))
                    continue;
            }
            else
            {
                lock(&kpt_lock);

                int j;
                for (j = 0; j < frames; j++)
                    if (kpt[i + j] != MAP_NP)
                        break;

                if (kpt[i + j] != MAP_NP)
                {
                    unlock(&kpt_lock);
                    i += j;
                    continue;
                }

                for (j = 0; j < frames; j++)
                    kpt[i + j] = pageframe_list[j] | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC;

                unlock(&kpt_lock);
            }

            return (void *)(HMEM_BASE + (uintptr_t)i * 4096);
        }
    }


    return NULL;
}


uintptr_t kernel_unmap(void *virt, size_t length)
{
    if ((uintptr_t)virt < HMEM_BASE)
        return (uintptr_t)virt;


    int bi = ((uintptr_t)virt - HMEM_BASE) >> 12;
    int pages = (length + ((uintptr_t)virt & 0xFFF) + 4095) >> 12;


    uintptr_t phys = (kpt[bi] & ~0xFFF) | ((uintptr_t)virt & 0xFFF);


    lock(&kpt_lock);

    for (int i = 0; i < pages; i++)
    {
        kpt[bi + i] = MAP_NP;
        invlpg(((uintptr_t)(bi + i) << 12) + HMEM_BASE);
    }

    unlock(&kpt_lock);


    return phys;
}


void *vmmc_user_map_sg(vmm_context_t *context, uintptr_t *phys, int pages, unsigned flags)
{
    int current_block_size = 0;


    lock(&context->arch.lock);


    int pdi, pti = 0;


    for (pdi = USER_MAP_BASE >> 22; pdi < (int)(KERNEL_BASE >> 22); pdi++)
    {
        uint32_t *pt;

        if (context->arch.pd[pdi] & MAP_PR)
        {
            pt = kernel_map(context->arch.pd[pdi] & ~0xFFF, 4096);

            for (pti = 0; (pti < 1024) && (current_block_size < pages); pti++)
            {
                if (pt[pti] & MAP_PR)
                    current_block_size = 0;
                else
                    current_block_size++;
            }

            kernel_unmap(pt, 4096);
        }
        else
        {
            current_block_size += 1024;

            pti = 1024;
        }

        // In der Schleifenbedingung wäre das doof, weil vorher noch inkrementiert würde.
        // Und das wäre doof, weil pti schon eins zu groß ist (und damit exakt das Ende
        // markiert).
        if (current_block_size >= pages)
            break;
    }


    if (current_block_size < pages)
    {
        unlock(&context->arch.lock);

        return NULL;
    }



    uintptr_t end = ((uintptr_t)pdi << 22) | ((uintptr_t)pti << 12);

    uintptr_t start = end - ((uintptr_t)current_block_size * 4096);

    // TODO: Muss aber auch nicht sein.
    for (int i = 0; i < pages; i++)
        vmmc_map_user_page_unlocked(context, (void *)(start + (ptrdiff_t)i * 4096), phys[i] & ~0xFFF, flags);


    unlock(&context->arch.lock);


    return (void *)start;
}


void *vmmc_user_map(vmm_context_t *context, uintptr_t phys, size_t length, unsigned flags)
{
    int pages = (length + (phys & 0xFFF) + 4095) >> 12;

    uintptr_t phys_sg[pages];

    for (int i = 0; i < pages; i++)
        phys_sg[i] = (phys & ~0xFFF) + (uintptr_t)i * 4096;

    return (void *)((uintptr_t)vmmc_user_map_sg(context, phys_sg, pages, flags) + (phys & 0xFFF));
}


void create_vmm_context_arch(vmm_context_t *context)
{
    context->arch.cr3 = pmm_alloc();
    context->arch.pd = kernel_map(context->arch.cr3, 4096);

    memcpy(context->arch.pd, kpd, 4096);

    lock_init(&context->arch.lock);
}


void destroy_vmm_context(vmm_context_t *context)
{
    vmmc_clear_user(context, false);

    lock(&context->arch.lock);

    kernel_unmap(context->arch.pd, 4096);
    pmm_free(context->arch.cr3);
}


void vmmc_map_user_page_unlocked(const vmm_context_t *context, void *virt, uintptr_t phys, unsigned flags)
{
    if (IS_KERNEL(virt))
        return;

    unsigned pdi = (uintptr_t)virt >> 22;
    unsigned pti = ((uintptr_t)virt >> 12) & 0x3FF;

    uint32_t *pt;

    if (context->arch.pd[pdi] & MAP_PR)
        pt = kernel_map(context->arch.pd[pdi] & ~0xFFF, 4096);
    else
    {
        unsigned lazy_flags = 0;
        if (context->arch.pd[pdi] & MAP_US)
            lazy_flags = (context->arch.pd[pdi] & MAP_RW) | MAP_US;

        context->arch.pd[pdi] = pmm_alloc() | MAP_PR | (lazy_flags ? lazy_flags : MAP_RW | MAP_US) | MAP_CC | MAP_LC | MAP_4K;
        pt = kernel_map(context->arch.pd[pdi] & ~0xFFF, 4096);
        memsetptr(pt, lazy_flags, 1024);
    }

    pt[pti] = (phys & ~0xFFF) | MAP_PR | (flags & (VMM_UW | VMM_PW) ? MAP_RW : 0) | (flags & (VMM_UR | VMM_UW | VMM_UX) ? MAP_US : 0) | MAP_CC | MAP_LC;

    kernel_unmap(pt, 4096);
}

void vmmc_map_user_page(vmm_context_t *context, void *virt, uintptr_t phys, unsigned flags)
{
    lock(&context->arch.lock);

    vmmc_map_user_page_unlocked(context, virt, phys, flags);

    unlock(&context->arch.lock);
}


void vmmc_lazy_map_area(vmm_context_t *context, void *virt, ptrdiff_t sz, unsigned flags)
{
    unsigned pdi = (uintptr_t)virt >> 22;
    unsigned pti = ((uintptr_t)virt >> 12) & 0x3FF;

    if (sz & (PAGE_SIZE - 1))
        sz = (sz >> PAGE_SHIFT) + 1;
    else
        sz >>= PAGE_SHIFT;

    lock(&context->arch.lock);

    while (sz > 0)
    {
        if (!(context->arch.pd[pdi] & MAP_PR) && ((unsigned)sz >= 1024) && !pti)
        {
            context->arch.pd[pdi++] = (flags & (VMM_UW | VMM_PW) ? MAP_RW : 0) | MAP_US | MAP_4K;
            sz -= 1024;
        }
        else
        {
            uint32_t *pt;

            if (context->arch.pd[pdi] & MAP_PR)
            {
                pt = kernel_map(context->arch.pd[pdi] & ~0xFFF, 4096);

                for (unsigned i = pti; (i < 1024) && (sz > 0); i++)
                {
                    if (!(pt[i] & MAP_PR))
                        pt[i] = (flags & (VMM_UW | VMM_PW) ? MAP_RW : 0) | MAP_US;

                    sz--;
                }
            }
            else
            {
                uintptr_t phys = pmm_alloc();
                context->arch.pd[pdi] = phys | MAP_PR | MAP_RW | MAP_US | MAP_CC | MAP_LC | MAP_4K;
                pt = kernel_map(phys, 4096);

                size_t memsetsz = 1024 - pti;
                if (memsetsz > (size_t)sz)
                    memsetsz = sz;

                memsetptr(pt + pti, (flags & (VMM_UW | VMM_PW) ? MAP_RW : 0) | MAP_US, memsetsz);

                sz -= memsetsz;
            }

            kernel_unmap(pt, 4096);

            pti = 0;
            pdi++;
        }
    }

    unlock(&context->arch.lock);
}


bool vmmc_do_cow(vmm_context_t *context, void *address)
{
    uintptr_t pdi = (uintptr_t)address >> 22;
    uintptr_t pti = ((uintptr_t)address >> 12) & 0x3FF;

    if (!(context->arch.pd[pdi] & MAP_PR))
        return false;


    uint32_t *pt = kernel_map(context->arch.pd[pdi] & ~0xFFF, 4096);

    if (!(pt[pti] & MAP_PR) || (pt[pti] & MAP_RW))
    {
        kernel_unmap(pt, 4096);
        return false;
    }


    if (!pmm_is_cow(pt[pti] & ~0xFFF))
    {
        kernel_unmap(pt, 4096);
        return false;
    }


    switch (pmm_user_count(pt[pti] & ~0xFFF))
    {
        case 0:
            kernel_unmap(pt, 4096);
            return false;

        case 1:
            pt[pti] |= MAP_RW;
            pmm_mark_cow(pt[pti] & ~0xFFF, false);

            kernel_unmap(pt, 1);

            // Spurious Pagefaults sind doof
            invlpg((uintptr_t)address);
            return true;
    }


    uintptr_t npf = pmm_alloc();

    void *np = kernel_map(npf, 4096);
    void *op = kernel_map(pt[pti] & ~0xFFF, 4096);


    memcpy(np, op, PAGE_SIZE);

    kernel_unmap(np, 1);
    kernel_unmap(op, 1);


    pmm_free(pt[pti] & ~0xFFF);

    pt[pti] = npf | MAP_PR | MAP_RW | MAP_US | MAP_CC | MAP_LC;


    kernel_unmap(pt, 4096);


    // Spurious Pagefault vermeiden
    invlpg((uintptr_t)address);


    return true;
}


bool vmmc_do_lazy_map(vmm_context_t *context, void *address)
{
    unsigned pdi = (uintptr_t)address >> 22;
    unsigned pti = ((uintptr_t)address >> 12) & 0x3FF;

    lock(&context->arch.lock);

    uint32_t pde = context->arch.pd[pdi];

    if (!(pde & MAP_US))
    {
        unlock(&context->arch.lock);
        return false;
    }

    uint32_t *pt;

    if (pde & MAP_PR)
        pt = kernel_map(pde & ~0xFFF, 4096);
    else
    {
        kassert(!(pde & MAP_4M));

        uintptr_t phys = pmm_alloc();
        context->arch.pd[pdi] = (pde & MAP_RW) | MAP_US | MAP_PR | MAP_CC | MAP_LC | MAP_4K | phys;
        pt = kernel_map(phys, 4096);

        memsetptr(pt, (pde & MAP_RW) | MAP_US, 1024);
    }

    if ((pt[pti] & (MAP_PR | MAP_US)) != MAP_US)
    {
        unlock(&context->arch.lock);

        kernel_unmap(pt, 4096);
        return false;
    }

    uintptr_t phys = pmm_alloc();
    pt[pti] = (pt[pti] & MAP_RW) | MAP_US | MAP_PR | MAP_CC | MAP_LC | phys;

    unlock(&context->arch.lock);

    kernel_unmap(pt, 4096);


    if (((uintptr_t)address >= USER_HERITAGE_BASE) && ((uintptr_t)address < USER_HERITAGE_TOP))
    {
        // Das wird zwischen Prozessen geteilt, also sollte der erste, der es
        // benutzt, irgendwas initialisiertes bekommen.
        void *virt = kernel_map(phys, 4096);
        memset(virt, 0, 4096);
        kernel_unmap(virt, 4096);
    }


    return true;
}


void vmmc_clear_user(vmm_context_t *context, bool preserve_heritage)
{
    lock(&context->arch.lock);

    for (unsigned pdi = 0; pdi < (PHYS_BASE >> 22); pdi++)
    {
        if (preserve_heritage && (pdi >= (USER_HERITAGE_BASE >> 22)) && (pdi < (USER_HERITAGE_TOP >> 22)))
            continue;

        if (!(context->arch.pd[pdi] & MAP_PR))
        {
            context->arch.pd[pdi] = MAP_NP;
            continue;
        }


        if ((pdi < (USER_MAP_BASE >> 22)) || (pdi >= (USER_MAP_TOP >> 22)))
        {
            // Mappings selber wurden physisch nicht vom Prozess reserviert und
            // dürfen damit hier auch nicht freigegeben werden.
            uint32_t *pt = kernel_map(context->arch.pd[pdi] & ~0xFFF, 4096);

            for (unsigned pti = 0; pti < 1024; pti++)
                if (pt[pti] & MAP_PR)
                    pmm_free(pt[pti] & ~0xFFF);

            kernel_unmap(pt, 4096);
        }


        pmm_free(context->arch.pd[pdi] & ~0xFFF);

        context->arch.pd[pdi] = MAP_NP;
    }

    unlock(&context->arch.lock);


    if (context == current_process->vmmc)
        full_tlb_invalidation();
}


void vmmc_clone(vmm_context_t *dest, vmm_context_t *source)
{
    bool current_affected = (current_process->vmmc == source) || (current_process->vmmc == dest);


    lock(&source->arch.lock);
    lock(&dest->arch.lock);


    for (unsigned pdi = 0; pdi < (PHYS_BASE >> 22); pdi++)
    {
        if (!(source->arch.pd[pdi] & MAP_PR))
        {
            dest->arch.pd[pdi] = source->arch.pd[pdi];
            continue;
        }


        uintptr_t nppt = pmm_alloc();

        dest->arch.pd[pdi] = nppt | MAP_PR | MAP_RW | MAP_US | MAP_CC | MAP_LC | MAP_4K;


        uint32_t *spt = kernel_map(source->arch.pd[pdi] & ~0xFFF, 4096);
        uint32_t *npt = kernel_map(nppt, 4096);


        for (unsigned pti = 0; pti < 1024; pti++)
        {
            if (!(spt[pti] & MAP_PR))
                npt[pti] = spt[pti];
            else
            {
                pmm_use(spt[pti] & ~0xFFF);
                pmm_mark_cow(spt[pti] & ~0xFFF, true);

                spt[pti] &= ~MAP_RW;

                npt[pti] = (spt[pti] & ~0xFFF) | MAP_PR | MAP_RO | MAP_US | MAP_CC | MAP_LC;


                if (current_affected)
                    invlpg(((uintptr_t)pdi << 22) + ((uintptr_t)pti << 12));
            }
        }


        kernel_unmap(spt, 4096);
        kernel_unmap(npt, 4096);
    }


    dest->heap_end = source->heap_end;


    unlock(&source->arch.lock);
    unlock(&dest->arch.lock);
}


void vmmc_user_unmap(vmm_context_t *context, void *virt, size_t length)
{
    unsigned pdi_start = (uintptr_t)virt >> 22;
    unsigned pdi_end = ((uintptr_t)virt + length + (1 << 22) - 1) >> 22;

    lock(&context->arch.lock);

    for (unsigned pdi = pdi_start; pdi < pdi_end; pdi++)
    {
        if (!(context->arch.pd[pdi] & MAP_PR))
            continue;

        unsigned pti_start = (pdi > pdi_start) ? 0 : ((uintptr_t)virt >> 12) & 0x3FF;
        unsigned pti_end = (pdi < pdi_end - 1) ? 1024 : (((uintptr_t)virt + length + 4095) >> 12) & 0x3FF;

        uint32_t *pt = kernel_map(context->arch.pd[pdi] & ~0xFFF, 4096);

        for (unsigned pti = pti_start; pti < pti_end; pti++)
        {
            pt[pti] = MAP_NP;
            invlpg(((uintptr_t)pdi << 22) + ((uintptr_t)pti << 12));
        }

        kernel_unmap(pt, 4096);

        if ((pti_start == 0) && (pti_end == 1024))
        {
            pmm_free(context->arch.pd[pdi] & ~0xFFF);
            context->arch.pd[pdi] = MAP_NP;
        }
    }

    unlock(&context->arch.lock);
}


unsigned vmmc_address_mapped(vmm_context_t *context, void *virt, uintptr_t *phys)
{
    unsigned pdi = (uintptr_t)virt >> 22;
    unsigned pti = ((uintptr_t)virt >> 12) & 0x3FF;

    lock(&context->arch.lock);

    if (!(context->arch.pd[pdi] & MAP_PR))
    {
        unlock(&context->arch.lock);
        return 0;
    }

    uint32_t *pt = kernel_map(context->arch.pd[pdi] & ~0xFFF, 4096);

    if (!(pt[pti] & MAP_PR))
    {
        kernel_unmap(pt, 4096);
        unlock(&context->arch.lock);
        return 0;
    }

    *phys = (pt[pti] & ~0xFFF) | ((uintptr_t)virt & 0xFFF);


    unsigned flags = 0;

    switch (pt[pti] & (MAP_RW | MAP_US))
    {
        case 0:
            flags = VMM_PR | VMM_PX;
            break;
        case MAP_RW:
            flags = VMM_PR | VMM_PW | VMM_PX;
            break;
        case MAP_US:
            flags = VMM_PR | VMM_PX | VMM_UR | VMM_UX;
            break;
        case MAP_RW | MAP_US:
            flags = VMM_PR | VMM_PW | VMM_PX | VMM_UR | VMM_UW | VMM_UX;
            break;
    }

    unlock(&context->arch.lock);


    kernel_unmap(pt, 4096);

    return flags;
}
