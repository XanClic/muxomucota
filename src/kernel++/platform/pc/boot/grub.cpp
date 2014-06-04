#include <cstdint>
#include <boot.hpp>
#include <memmap.hpp>
#include <parith.hpp>
#include <vmem.hpp>

#include <arch-constants.h>


static uint32_t mbhdr[] __attribute__((section(".multiboot"), used)) = {
    0x1badb002,
    0x00000003,
    static_cast<uint32_t>(-(0x1badb002 + 0x00000003))
};


extern const char __kernel_elf_end;


void *kernel_elf_shdr;
int kernel_elf_shdr_count;
memory_map memmap;

static multiboot_info *mboot;
static bios_memory_map *mmap;

static multiboot_module *modules;
static int module_count;


bool boot_info::inspect(void)
{
    // Test for GRUB
    if (bl_id != 0x2badb002) {
        return false;
    }


    mboot = kernel_map<multiboot_info>(mboot_paddr);


    // Test for memory map flag
    if (!(mboot->mi_flags & (1 << 6))) {
        return false;
    }

    mmap = static_cast<bios_memory_map *>(kernel_map(mboot->mmap_addr, mboot->mmap_length));

    int mmap_entries = 0;
    for (bios_memory_map *entry = mmap; entry - mmap < static_cast<ptrdiff_t>(mboot->mmap_length); mmap_entries++) {
        entry = byte_ptr(entry) + entry->size + sizeof(entry->size);
    }
    memmap.size.set(mmap_entries);


    // Test for module flag
    if (mboot->mi_flags & (1 << 3)) {
        modules = static_cast<multiboot_module *>(kernel_map(mboot->mods_addr, sizeof(multiboot_module) * mboot->mods_count));
        module_count = mboot->mods_count;
    } else {
        modules = nullptr;
        module_count = 0;
    }


    kernel_elf_shdr = kernel_map(mboot->elfshdr_addr, sizeof(const char *) * mboot->elfshdr_num);
    kernel_elf_shdr_count = mboot->elfshdr_num;


    return true;
}


const char *boot_info::command_line(void)
{
    static const char *cmdline;

    if (!cmdline) {
        // FIXME
        cmdline = static_cast<const char *>(kernel_map(mboot->cmdline, 512));
    }

    return cmdline;
}


memory_map::entry memory_map::operator[](int i)
{
    if ((i < 0) || (i >= size)) {
        return memory_map::entry(false, 0, 0);
    }

    bios_memory_map *e = mmap;

    while (i--) {
        e = byte_ptr(e) + e->size + sizeof(e->size);
    }

    if (e->base > UINTPTR_MAX) {
        return memory_map::entry(false, 0, 0);
    }

    return memory_map::entry(
            e->type == 1,
            static_cast<uintptr_t>(e->base),
            (e->length > SIZE_MAX) ? SIZE_MAX : e->length
    );
}


uintptr_t memory_map::kernel_start(void)
{
    // Who needs lower memory anyway
    return PHYS_BASE;
}


uintptr_t memory_map::kernel_end(void)
{
    uintptr_t e = reinterpret_cast<uintptr_t>(&__kernel_elf_end);

    for (int i = 0; i < module_count; i++) {
        if (modules[i].mod_end + PHYS_BASE > e) {
            e = modules[i].mod_end + PHYS_BASE;
        }
    }

    if (reinterpret_cast<uintptr_t>(mmap) + mboot->mmap_length > e) {
        e = reinterpret_cast<uintptr_t>(mmap) + mboot->mmap_length;
    }

    if (reinterpret_cast<uintptr_t>(modules + module_count) > e) {
        e = reinterpret_cast<uintptr_t>(modules + module_count);
    }

    return e;
}
