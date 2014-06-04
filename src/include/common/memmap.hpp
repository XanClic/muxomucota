#ifndef _MEMMAP_HPP
#define _MEMMAP_HPP

#include <cstddef>
#include <cstdint>
#include <voodoo>


struct memory_map {
    struct entry {
        entry(bool u, uintptr_t s, size_t l): usable(u), start(s), length(l) {}

        bool usable;
        uintptr_t start;
        size_t length;
    };

    const_attribute<int> size;

    entry operator[](int index);

    static uintptr_t kernel_start(void);
    static uintptr_t kernel_end(void);
};


extern memory_map memmap;

#endif
