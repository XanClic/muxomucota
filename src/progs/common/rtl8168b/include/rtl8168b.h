/*
 * Copyright (c) 2007 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Patrick Pokatilo.
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

#ifndef _RTL8168B_H_
#define _RTL8168B_H_

#include <stdint.h>

#include "cdi.h"
#include "cdi/mem.h"
#include "cdi/net.h"
#include "cdi/pci.h"

#define REG_ID0                     0x00
#define REG_TRANSMIT_DESCRIPTORS    0x20
#define REG_COMMAND                 0x37
#define REG_TRANSMIT_POLL           0x38
#define REG_INTERRUPT_MASK          0x3C
#define REG_INTERRUPT_STATUS        0x3E
#define REG_TRANSMIT_CONFIGURATION  0x40
#define REG_RECEIVE_CONFIGURATION   0x44
#define REG_PHY_STATUS              0x6C
#define REG_RECEIVE_DESCRIPTORS     0xE4

#define CR_RESET                (1 << 4)
#define CR_RECEIVER_ENABLE      (1 << 3)
#define CR_TRANSMITTER_ENABLE   (1 << 2)

#define TPR_PACKET_WAITING      (1 << 6)

#define TCR_IFG_STANDARD        (3 << 24)
#define TCR_MXDMA_512           (5 << 8)
#define TCR_MXDMA_1024          (6 << 8)
#define TCR_MXDMA_UNLIMITED     (7 << 8)

#define RCR_MXDMA_512           (5 << 8)
#define RCR_MXDMA_1024          (6 << 8)
#define RCR_MXDMA_UNLIMITED     (7 << 8)
#define RCR_ACCEPT_BROADCAST    (1 << 3)
#define RCR_ACCEPT_MULTICAST    (1 << 2)
#define RCR_ACCEPT_PHYS_MATCH   (1 << 1)

#define ISR_LINK_CHANGE             (1 << 5)
#define ISR_RECEIVE_BUFFER_OVERFLOW (1 << 4)
#define ISR_TRANSMIT_OK             (1 << 2)
#define ISR_RECEIVE_OK              (1 << 0)

#define DC_OWN  (1 << 31)
#define DC_EOR  (1 << 30)
#define DC_FS   (1 << 29)
#define DC_LS   (1 << 28)
#define DC_LENGTH  0x3FFF

#define PS_1000     (1 << 4)
#define PS_100      (1 << 3)
#define PS_10       (1 << 2)
#define PS_LINK     (1 << 1)
#define PS_FULLDUP  (1 << 0)

#define PHYS(netcard, field) \
    (netcard->phys + offsetof(struct rtl8168b_device, field))

#define TX_BUFFER_SIZE  (0x1000 & DC_LENGTH)
#define RX_BUFFER_SIZE  (0x2000 & DC_LENGTH)

#define TX_BUFFER_COUNT 1
#define RX_BUFFER_COUNT 1

struct rtl8168b_descriptor
{
    uint32_t command;
    uint32_t vlan;
    uint64_t address;
};

struct rtl8168b_device {
    struct cdi_net_device       net;
    uintptr_t                   phys;
    uint16_t                    port_base;
    uint8_t                     lock;
    struct rtl8168b_descriptor  tx_buffer[TX_BUFFER_COUNT]
                                    __attribute__ ((aligned (256)));
    struct cdi_mem_area*        tx_area[TX_BUFFER_COUNT];
    struct rtl8168b_descriptor  rx_buffer[RX_BUFFER_COUNT]
                                    __attribute__ ((aligned (256)));
    struct cdi_mem_area*        rx_area[RX_BUFFER_COUNT];
    cdi_list_t                  pending_sends;
};

struct cdi_device* rtl8168b_init_device(struct cdi_bus_data* bus_data);
void rtl8168b_remove_device(struct cdi_device* device);
void rtl8168b_send_packet(struct cdi_net_device* device,
    void* data, size_t size);

#endif
