#include <arch-constants.h>
#include <arch-vmem.h>
#include <elf32.h>
#include <pmm.h>
#include <process.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vmem.h>


pid_t create_process_from_image(const char *name, const void *address, size_t size)
{
    const Elf32_Ehdr *ehdr = address;

    if ((ehdr->e_ident[0] != 0x7F) ||
        (ehdr->e_ident[1] !=  'E') ||
        (ehdr->e_ident[2] !=  'L') ||
        (ehdr->e_ident[3] !=  'F') ||
        (ehdr->e_ident[EI_CLASS]   != ELFCLASS32)  ||
        (ehdr->e_ident[EI_VERSION] != EV_CURRENT)  ||
        (ehdr->e_ident[EI_DATA]    != ELFDATA2LSB) ||
        (ehdr->e_type    != ET_EXEC) ||
        (ehdr->e_machine != EM_THIS))
    {
        // Nicht unterstützter (Datei-)Typ
        return -1;
    }


    process_t *proc = create_empty_process(name);


    const Elf32_Phdr *phdr = (const Elf32_Phdr *)((uintptr_t)ehdr + ehdr->e_phoff);

    uintptr_t image_end = 0;


    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        ptrdiff_t block_ofs = phdr[i].p_vaddr & (PAGE_SIZE - 1); // block (memory) offset
        size_t rem_msz = phdr[i].p_memsz; // remaining memory size
        size_t rem_fsz = phdr[i].p_filesz; // remaining file size
        void *source = (void *)((uintptr_t)address + phdr[i].p_offset); // current source address


        // image_end anpassen, wenn nötig
        if (phdr[i].p_vaddr + rem_msz > image_end)
            image_end = phdr[i].p_vaddr + rem_msz;


        if (rem_fsz > rem_msz)
            rem_fsz = rem_msz;


        for (uintptr_t base = phdr[i].p_vaddr & ~(PAGE_SIZE - 1); rem_msz > 0; base += PAGE_SIZE)
        {
            uintptr_t ppf = pmm_alloc(1); // physical page frame

            size_t block_length = PAGE_SIZE - block_ofs;

            if (block_length > rem_msz)
                block_length = rem_msz;


            void *vpf = kernel_map(ppf + block_ofs, block_length); // virtual page frame


            memset(vpf, 0, block_length);

            rem_msz -= block_length;


            if (block_length > rem_fsz)
                block_length = rem_fsz;

            if (block_length)
            {
                memcpy(vpf, source, block_length);

                rem_fsz -= block_length;
                source = (void *)((uintptr_t)source + block_length);
            }


            block_ofs = 0;


            kernel_unmap(vpf, block_length);


            // TODO: Flags anpassen
            vmmc_map_user_page(proc->vmmc, (void *)base, ppf, VMM_UR | VMM_UW | VMM_UX);
        }
    }


    vmmc_set_heap_top(proc->vmmc, (void *)image_end);


    vmmc_lazy_map_area(proc->vmmc, (void *)USER_STACK_BASE, USER_STACK_TOP - USER_STACK_BASE, VMM_UR | VMM_UW);

    make_process_entry(proc, (void (*)(void))ehdr->e_entry, (void *)USER_STACK_TOP);


    register_process(proc);

    return proc->pid;
}
