#ifndef _PMM_HPP
#define _PMM_HPP

#include <cstddef>
#include <cstdint>
#include <vmem.hpp>
#include <voodoo>


class physptr {
    public:
        physptr(void) {}
        physptr(uintptr_t adr): address(adr) {}
        physptr(std::nullptr_t): address(0) {}

        operator uintptr_t(void) { return address; }

        static physptr alloc(int frame_count = 1, uintptr_t max_addr = UINTPTR_MAX, int alignment = 0);
        void free(void);

        bool alloced(void);

        gs_attribute(bool, cow);
        gs_attribute(int, users);

    private:
        uintptr_t address;

        bool get_cow(void);
        void set_cow(bool flag);
        int get_users(void);
        int inc_users(void);
        int dec_users(void);
};


void init_pmm(void);

#endif
