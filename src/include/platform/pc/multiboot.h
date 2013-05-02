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

#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

#include <compiler.h>
#include <stdint.h>

struct multiboot_module
{
    uint32_t mod_start; // void *
    uint32_t mod_end;   // void *
    uint32_t string;    // const char *
    uint8_t reserved[4];
} cc_packed;

struct apm_table
{
    uint16_t version;
    uint16_t cseg;
    uint32_t offset; // void *
    uint16_t cseg16;
    uint16_t flags;
    uint16_t cseg_len;
    uint16_t cseg16_len;
    uint16_t dseg_len;
} cc_packed;

struct memory_map
{
    uint32_t size;
    uint64_t base;
    uint64_t length;
    uint32_t type;
} cc_packed;

struct multiboot_info
{
    uint32_t mi_flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;          // const char *
    uint32_t mods_count;
    uint32_t mods_addr;        // struct multiboot_module *
    uint32_t elfshdr_num;
    uint32_t elfshdr_size;
    uint32_t elfshdr_addr;
    uint32_t elfshdr_shndx;
    uint32_t mmap_length;
    uint32_t mmap_addr;        // struct memory_map *
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;     // void *
    uint32_t boot_loader_name; // const char *
    uint32_t apm_table;        // struct apm_table *
    uint32_t vbe_control_info; // void *
    uint32_t vbe_mode_info;    // void *
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} cc_packed;

#endif
