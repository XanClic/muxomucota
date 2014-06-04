#ifndef _PLAT_BOOT_HPP
#define _PLAT_BOOT_HPP

#include <cstdint>

#include <multiboot.hpp>


class platform_boot_info {
    protected:
        uint32_t bl_id;
        uint32_t mboot_paddr;
};

#endif
