#include <boot.h>
#include <memmap.h>
#include <multiboot.h>
#include <prime-procs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <arch-constants.h>


static uint32_t mbhdr[] __attribute__((section(".multiboot"), used)) = { 0x1BADB002, 0x00000003, (uint32_t)-(0x1BADB002 + 0x00000003) };


static struct memory_map *mmap;
static int mmap_length;

static struct multiboot_module *modules;
static int module_count;

extern const void __kernel_elf_end;


bool get_boot_info(void *info)
{
    // Ist das auch GRUB?
    if (*(uint32_t *)info != 0x2BADB002)
        return false;


    struct multiboot_info *mboot = (struct multiboot_info *)(((uint32_t *)info)[1] | PHYS_BASE);


    if (!(mboot->mi_flags & (1 << 6))) // Memory Map
        return false;

    mmap = (struct memory_map *)(mboot->mmap_addr | PHYS_BASE);

    for (struct memory_map *entry = mmap; entry - mmap < (int)mboot->mmap_length; mmap_length++)
        entry = (struct memory_map *)((uintptr_t)entry + entry->size + sizeof(entry->size));


    if (mboot->mi_flags & (1 << 3)) // Module
    {
        modules = (struct multiboot_module *)((uintptr_t)mboot->mods_addr | PHYS_BASE);
        module_count = mboot->mods_count;
    }
    else
    {
        modules = NULL;
        module_count = 0;
    }


    return true;
}


int memmap_length(void)
{
    return mmap_length;
}


void get_memmap_info(int index, bool *usable, uintptr_t *start, size_t *length)
{
    if ((index < 0) || (index >= mmap_length))
        goto unusable;


    struct memory_map *entry = mmap;

    while (index--)
        entry = (struct memory_map *)((uintptr_t)entry + entry->size + sizeof(entry->size));


    if (entry->base >> 32)
        goto unusable;


    *usable = (entry->type == 1);
    *start  = (uintptr_t)entry->base;

    *length = (entry->length >> 32) ? 0xFFFFFFFF : (size_t)entry->length;

    return;


unusable:
    *usable = false;
    *start = 0;
    *length = 0;
}


void get_kernel_dim(uintptr_t *start, uintptr_t *end)
{
    *start = PHYS_BASE; // Wer braucht schon Lower Memory

    uintptr_t e = (uintptr_t)&__kernel_elf_end;

    for (int i = 0; i < module_count; i++)
        if (modules[i].mod_end + 1 + PHYS_BASE > e)
            e = modules[i].mod_end + 1 + PHYS_BASE;

    if ((uintptr_t)(mmap + mmap_length) > e)
        e = (uintptr_t)(mmap + mmap_length);

    if ((uintptr_t)(modules + module_count) > e)
        e = (uintptr_t)(modules + module_count);

    *end = e;
}


int prime_process_count(void)
{
    return module_count;
}


void fetch_prime_process(int index, void **start, size_t *length, char *name_array, size_t name_array_size)
{
    if ((index < 0) || (index >= module_count))
    {
        *start = NULL;
        *length = 0;
        return;
    }


    *start  = (void *)(modules[index].mod_start | PHYS_BASE);
    *length = modules[index].mod_start + 1 - modules[index].mod_end;

    if (!name_array_size)
        return;


    const char *src = (const char *)(modules[index].string | PHYS_BASE);

    int i;
    for (i = 0; (i < (int)name_array_size - 1) && src[i] && (src[i] != ' '); i++)
        name_array[i] = src[i];
    name_array[i] = 0;
}
