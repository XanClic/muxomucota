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

#ifndef _DRIVERS__PCI_H
#define _DRIVERS__PCI_H

#include <compiler.h>
#include <stdint.h>


struct pci_config_space
{
    uint16_t vendor_id, device_id;
    uint16_t command, status;
    uint8_t revision, interface, subclass, baseclass;
    uint8_t cache_line_size, latency_timer, header_type, bist;

    union
    {
        struct
        {
            uint32_t bar[6];
            uint32_t cardbus_cis;
            uint16_t subsystem_vendor_id, subsystem_device_id;
            uint32_t expansion_rom;
            uint8_t capabilities;
            uint8_t rsvd0, rsvd1, rsvd2;
            uint32_t rsvd3;
            uint8_t interrupt_line, interrupt_pin;
            uint8_t min_grant, max_latency;
        } hdr0;

        struct
        {
            uint32_t bar[2];
            uint8_t primary_bus_nr, secondary_bus_nr, subordinate_bus_nr, secondary_latency_timer;
            uint8_t io_base, io_limit;
            uint16_t secondary_status;
            uint16_t memory_base, memory_limit;
            uint16_t prefetch_memory_base, prefetch_memory_limit;
            uint32_t prefetch_base_upper, prefetch_limit_upper;
            uint16_t io_base_upper, io_limit_upper;
            uint8_t capabilities;
            uint8_t rsvd0, rsvd1, rsvd2;
            uint32_t extension_rom_base;
            uint8_t interrupt_line, interrupt_pin;
            uint16_t bridge_control;
        } hdr1;

        struct
        {
            uint32_t bar[2];
            uint32_t rsvd[7];
            uint8_t capabilities;
            uint8_t rsvd0, rsvd1, rsvd2;
            uint32_t rsvd3;
            uint8_t interrupt_line, interrupt_pin;
            uint16_t rsvd4;
        } common;
    };
} cc_packed;

#endif
