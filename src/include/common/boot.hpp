#ifndef _BOOT_HPP
#define _BOOT_HPP

#include <plat-boot.hpp>

class boot_info: protected platform_boot_info {
    public:
        bool inspect(void);
        const char *command_line(void);
};

#endif
