/*
 * Copyright (c) 2007 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Matthew Iselin.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NE2K_H_
#define _NE2K_H_

#include <stdint.h>

#include "cdi.h"
#include "cdi/net.h"

#define NE_CMD      0
#define NE_PSTART   1
#define NE_PSTOP    2
#define NE_BNDRY    3

#define NE_TSR      4 // R
#define NE_TPSR     4 // W

#define NE_TBCR0    5
#define NE_TBCR1    6

#define NE_ISR      7

#define NE_RSAR0    8
#define NE_RSAR1    9
#define NE_RBCR0    10
#define NE_RBCR1    11

#define NE_RCR      12
#define NE_TCR      13
#define NE_DCR      14
#define NE_IMR      15

// Register page 1
#define NE_PAR      1 // PAR0..5
#define NE_CURR     7
#define NE_MAR      8

// Packet ring buffer offsets
#define PAGE_TX     0x40
#define PAGE_RX     0x50
#define PAGE_STOP   0x80

#define NE_RESET    0x1F
#define NE_DATA     0x10

#define PHYS(netcard, field) \
    (netcard->phys + offsetof(struct rtl8139_device, field))

#define RX_BUFFER_SIZE 0x2000
#define TX_BUFFER_SIZE 0x1000

typedef struct {
    void* virt;
    uintptr_t phys;
} cdi_dma_mem_ptr_t;

struct ne2k_device {
    struct cdi_net_device       net;

    uintptr_t                   phys;
    uint16_t                    port_base;

    uint8_t                     next_packet;

    int                         tx_in_progress;

    cdi_list_t                  pending_sends;
};

struct cdi_device* ne2k_init_device(struct cdi_bus_data* bus_data);
void ne2k_remove_device(struct cdi_device* device);

void ne2k_send_packet
    (struct cdi_net_device* device, void* data, size_t size);

#endif
