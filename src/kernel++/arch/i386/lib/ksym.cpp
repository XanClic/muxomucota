#include <cstddef>
#include <cstdint>
#include <cstring>
#include <elf32.hpp>
#include <ksym.hpp>
#include <arch-constants.hpp>


extern Elf32_Shdr *kernel_elf_shdr;
extern int kernel_elf_shdr_count;


bool kernel_find_function(uintptr_t addr, char *name, uintptr_t *func_base)
{
    Elf32_Sym *elf_sym = nullptr;
    char *strtbl = nullptr;
    int syms = 0;


    for (int i = 0; i < kernel_elf_shdr_count; i++) {
        if (kernel_elf_shdr[i].sh_type == SH_SYMTAB) {
            // FIXME: Actually this would have to use kernel_map(), but as this
            // function is generally called when something very bad happened (to
            // generate a debug trace), that might make things worse.
            elf_sym = reinterpret_cast<Elf32_Sym *>(kernel_elf_shdr[i].sh_addr | PHYS_BASE);
            syms = kernel_elf_shdr[i].sh_size / sizeof(Elf32_Sym);

            strtbl = reinterpret_cast<char *>(kernel_elf_shdr[kernel_elf_shdr[i].sh_link].sh_addr | PHYS_BASE);

            break;
        }
    }


    if (!elf_sym) {
        return false;
    }


    uintptr_t best_hit = 0;
    int best_hit_idx = -1;

    for (int i = 0; i < syms; i++) {
        if (((ELF32_ST_TYPE(elf_sym[i].st_info) == STT_FUNC) ||
             (ELF32_ST_TYPE(elf_sym[i].st_info) == STT_OBJECT)) &&
             (elf_sym[i].st_value <= addr) &&
             (elf_sym[i].st_value >  best_hit))
        {
            best_hit = elf_sym[i].st_value;
            best_hit_idx = i;
        }
    }

    if (best_hit_idx < 0) {
        return false;
    }


    *func_base = best_hit;
    strcpy(name, &strtbl[elf_sym[best_hit_idx].st_name]);

    return true;
}
