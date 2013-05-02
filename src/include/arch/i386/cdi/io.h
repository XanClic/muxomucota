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

#ifndef _CDI_IO_H_
#define _CDI_IO_H_

#include <drivers/ports.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint8_t  cdi_inb(uint16_t port) { return in8(port);  }
static inline uint16_t cdi_inw(uint16_t port) { return in16(port); }
static inline uint32_t cdi_inl(uint16_t port) { return in32(port); }

static inline void cdi_outb(uint16_t port, uint8_t data)  { out8(port, data);  }
static inline void cdi_outw(uint16_t port, uint16_t data) { out16(port, data); }
static inline void cdi_outl(uint16_t port, uint32_t data) { out32(port, data); }

#ifdef __cplusplus
}; // extern "C"
#endif

#endif

