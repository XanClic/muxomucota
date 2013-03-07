/*
 * Copyright (c) 2007 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Patrick Pokatilo and Max Reitz.
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <cdi.h>
#include <cdi/mem.h>
#include <cdi/misc.h>
#include <cdi/net.h>
#include <cdi/pci.h>

#include "rtl8168b.h"

#define DEBUG_MSG(s) \
    printf("[RTL8111B/8168B] debug: %s() '%s'\n", __FUNCTION__, s)
#define DEVICE_MSG printf

static void rtl8168b_handle_interrupt(struct cdi_device* device);

static inline void write_register_8(struct rtl8168b_device* netcard,
    uint8_t reg, uint8_t value)
{
    *(volatile uint8_t *)((uintptr_t)netcard->mmio + reg) = value;
}

static inline void write_register_16(struct rtl8168b_device* netcard,
    uint8_t reg, uint16_t value)
{
    *(volatile uint16_t *)((uintptr_t)netcard->mmio + reg) = value;
}

static inline void write_register_32(struct rtl8168b_device* netcard,
    uint8_t reg, uint32_t value)
{
    *(volatile uint32_t *)((uintptr_t)netcard->mmio + reg) = value;
}

static inline void write_register_64(struct rtl8168b_device* netcard,
    uint8_t reg, uint64_t value)
{
    *(volatile uint64_t *)((uintptr_t)netcard->mmio + reg) = value;
}

static inline uint8_t read_register_8(struct rtl8168b_device* netcard,
    uint8_t reg)
{
    return *(volatile uint8_t *)((uintptr_t)netcard->mmio + reg);
}

static inline uint16_t read_register_16(struct rtl8168b_device* netcard,
    uint8_t reg)
{
    return *(volatile uint16_t *)((uintptr_t)netcard->mmio + reg);
}

static inline uint32_t read_register_32(struct rtl8168b_device* netcard,
    uint8_t reg)
{
    return *(volatile uint32_t *)((uintptr_t)netcard->mmio + reg);
}

static inline uint64_t read_register_64(struct rtl8168b_device* netcard,
    uint8_t reg)
{
    return *(volatile uint64_t *)((uintptr_t)netcard->mmio + reg);
}


struct cdi_device* rtl8168b_init_device(struct cdi_bus_data* bus_data)
{
    struct cdi_pci_device* pci = (struct cdi_pci_device*) bus_data;
    struct rtl8168b_device* netcard;
    struct cdi_mem_area* buf;

    if ((pci->vendor_id != 0x10ec) || (pci->device_id != 0x8168)) {
        return NULL;
    }


    buf = cdi_mem_alloc(sizeof(*netcard), CDI_MEM_VIRT_ONLY | CDI_MEM_NOINIT);

    if (!buf) {
        return NULL;
    }


    netcard = buf->vaddr;

    memset(netcard, 0, sizeof(*netcard));


    netcard->net.dev.bus_data = (struct cdi_bus_data*) pci;

    DEBUG_MSG("Interrupthandler registrieren");
    cdi_register_irq(pci->irq, rtl8168b_handle_interrupt, &netcard->net.dev);
    cdi_pci_alloc_memory(pci);


    struct cdi_pci_resource* res;
    for (int i = 0; (res = cdi_list_get(pci->resources, i)); i++) {
        if ((res->type == CDI_PCI_MEMORY) && (res->index == 2)) {
            netcard->mmio = res->address;
            break;
        }
    }


    DEBUG_MSG("Reset der Karte");
    write_register_16(netcard, REG_CP_COMMAND, CPCR_VLAN_DETAG);

    write_register_8(netcard, REG_COMMAND, CR_RESET);

    while ((read_register_8(netcard, REG_COMMAND) & CR_RESET) == CR_RESET);

    write_register_16(netcard, REG_RECV_MAX_SIZE, 1518);
    write_register_16(netcard, REG_MAX_TRANS_SIZE, 0x0c);

    DEBUG_MSG("Aktiviere Rx/Tx");
    write_register_8(netcard, REG_COMMAND,
        CR_RECEIVER_ENABLE | CR_TRANSMITTER_ENABLE);

    DEBUG_MSG("MAC-Adresse auslesen");
    netcard->net.mac =
        read_register_64(netcard, REG_ID0) & 0xffffffffffffLL;

    DEBUG_MSG("Setze Interruptmaske");
    write_register_16(netcard, REG_INTERRUPT_STATUS, 0x4fff);
    write_register_16(netcard, REG_INTERRUPT_MASK, 0x0025);

    DEBUG_MSG("Initialisiere PHY");
    write_register_32(netcard, REG_PHY_ACCESS,
        PA_RW | (PHY_REG_BMCR << 16) | PHY_BMCR_RESET);

    while (read_register_32(netcard, REG_PHY_ACCESS) & PA_RW);

    do {
        write_register_16(netcard, REG_PHY_ADDRESS, PHY_REG_BMCR);

        while (!(read_register_8(netcard, REG_PHY_RW) >> 7));
    } while (read_register_16(netcard, REG_PHY_DATA) & PHY_BMCR_RESET);

    write_register_32(netcard, REG_PHY_ACCESS,
        PA_RW | (PHY_REG_BMCR << 16) | PHY_BMCR_AUTO_NEGOTIATION);

    while (read_register_32(netcard, REG_PHY_ACCESS) & PA_RW);


    DEVICE_MSG("Lege Deskriptoren an");
    netcard->tx_buffer_area =
        cdi_mem_alloc(TX_BUFFER_COUNT * sizeof(struct rtl8168b_descriptor),
        CDI_MEM_PHYS_CONTIGUOUS | 8);
    netcard->tx_buffer = netcard->tx_buffer_area->vaddr;
    netcard->tx_buffer_phys = netcard->tx_buffer_area->paddr.items[0].start;

    netcard->rx_buffer_area =
        cdi_mem_alloc(RX_BUFFER_COUNT * sizeof(struct rtl8168b_descriptor),
        CDI_MEM_PHYS_CONTIGUOUS | 8);
    netcard->rx_buffer = netcard->rx_buffer_area->vaddr;
    netcard->rx_buffer_phys = netcard->rx_buffer_area->paddr.items[0].start;

    int i;
    for (i = 0; i < RX_BUFFER_COUNT; i++) {
        memset(&netcard->rx_buffer[i], 0, sizeof(struct rtl8168b_descriptor));

        netcard->rx_area[i] =
            cdi_mem_alloc(RX_BUFFER_SIZE, CDI_MEM_PHYS_CONTIGUOUS | 3);
        netcard->rx_buffer[i].address =
            netcard->rx_area[i]->paddr.items[0].start;
        netcard->rx_buffer[i].command = DC_OWN | RX_BUFFER_SIZE;

        if (i == RX_BUFFER_COUNT - 1) {
            netcard->rx_buffer[i].command |= DC_EOR;
        }
    }

    if (i == 0) {
        DEVICE_MSG("[RTL8111B/8168B] Keine Buffer zum Empfangen von Daten!\n");
        write_register_8(netcard, REG_COMMAND,
            read_register_8(netcard, REG_COMMAND) & ~CR_RECEIVER_ENABLE);
    }

    for (i = 0; i < TX_BUFFER_COUNT; i++) {
        memset(&netcard->tx_buffer[i], 0, sizeof(struct rtl8168b_descriptor));

        netcard->tx_area[i] =
            cdi_mem_alloc(TX_BUFFER_SIZE, CDI_MEM_PHYS_CONTIGUOUS | 3);
        netcard->tx_buffer[i].address =
            netcard->tx_area[i]->paddr.items[0].start;
        netcard->tx_buffer[i].command  = 0;

        if (i == TX_BUFFER_COUNT - 1) {
            netcard->tx_buffer[i].command |= DC_EOR;
        }
    }

    if (i == 0) {
        DEVICE_MSG("[RTL8111B/8168B] Keine Buffer zum Senden von Daten!\n");
        write_register_8(netcard, REG_COMMAND,
            read_register_8(netcard, REG_COMMAND) & ~CR_TRANSMITTER_ENABLE);
    }

    DEBUG_MSG("Initialisiere Buffer");
    write_register_64(netcard, REG_RECEIVE_DESCRIPTORS,
        netcard->rx_buffer_phys);
    write_register_64(netcard, REG_TRANSMIT_DESCRIPTORS,
        netcard->tx_buffer_phys);

    DEBUG_MSG("Initialisiere RCR/TCR");
    write_register_32(netcard, REG_RECEIVE_CONFIGURATION,
        RCR_MXDMA_UNLIMITED | RCR_NO_FIFO_THRESHOLD | RCR_ACCEPT_BROADCAST |
        RCR_ACCEPT_PHYS_MATCH);
    write_register_32(netcard, REG_TRANSMIT_CONFIGURATION,
        TCR_MXDMA_UNLIMITED | TCR_IFG_STANDARD);

    cdi_net_device_init(&netcard->net);

    DEBUG_MSG("Fertig initialisiert");
    return &netcard->net.dev;
}

void rtl8168b_remove_device(struct cdi_device* device)
{
    struct rtl8168b_device* netcard = (struct rtl8168b_device*) device;
    int i;

    cdi_pci_free_memory((struct cdi_pci_device*) netcard->net.dev.bus_data);

    cdi_mem_free(netcard->tx_buffer_area);
    cdi_mem_free(netcard->rx_buffer_area);

    for (i = 0; i < RX_BUFFER_COUNT; i++) {
        netcard->rx_buffer[i].command &= 0x40000000;
        netcard->rx_buffer[i].address = (uintptr_t) NULL;

        cdi_mem_free(netcard->rx_area[i]);
    }

    for (i = 0; i < TX_BUFFER_COUNT; i++) {
        netcard->tx_buffer[i].command &= 0x40000000;
        netcard->tx_buffer[i].address = (uintptr_t) NULL;

        cdi_mem_free(netcard->tx_area[i]);
    }
}

void rtl8168b_send_packet(struct cdi_net_device* device,
    void *data, size_t size)
{
    struct rtl8168b_device* netcard = (struct rtl8168b_device*) device;

    if (size > TX_BUFFER_SIZE) {
        DEVICE_MSG("[RTL8111B/8168B] Paket zu groß!\n");

        return;
    }

    int i;

    do {
        i = netcard->tx_index;
    } while (!__sync_bool_compare_and_swap(&netcard->tx_index, i,
        (i + 1) % TX_BUFFER_COUNT));

    if (netcard->tx_buffer[i].command & DC_OWN) {
        DEBUG_MSG("Tx-Buffer ist schon besetzt");
        return;
    }


    memcpy(netcard->tx_area[i]->vaddr, data, size);
    netcard->tx_buffer[i].vlan = 0;

    DEVICE_MSG("[RTL8111B/8168B] Sende via Deskriptor %i\n", i);

    if (i == TX_BUFFER_COUNT - 1) {
        netcard->tx_buffer[i].command = size | DC_FS | DC_LS | DC_OWN | DC_EOR;
    } else {
        netcard->tx_buffer[i].command = size | DC_FS | DC_LS | DC_OWN;
    }

    write_register_8(netcard, REG_TRANSMIT_POLL, TPR_PACKET_WAITING);
}


static void rtl8168b_unown_rx_desc(struct rtl8168b_device* netcard, int i)
{
    if (i == RX_BUFFER_COUNT - 1) {
        netcard->rx_buffer[i].command = RX_BUFFER_SIZE | DC_OWN | DC_EOR;
    } else {
        netcard->rx_buffer[i].command = RX_BUFFER_SIZE | DC_OWN;
    }
}

static void rtl8168b_handle_receive_ok(struct rtl8168b_device* netcard)
{
    int i;

    do {
        i = netcard->rx_index;
    } while (!__sync_bool_compare_and_swap(&netcard->rx_index, i,
        (i + 1) % RX_BUFFER_COUNT));

    if (netcard->rx_buffer[i].command & DC_OWN) {
        netcard->rx_index = i;

        if (!(netcard->rx_buffer[0].command & DC_OWN)) {
            // FIXME: Make atomic
            i = 0;
            netcard->rx_index = 1;
        } else {
            DEVICE_MSG("[RTL8111B/8168B] Empfangsdeskriptor sollte %i sein, "
                "dort ist aber nichts.\n", i);
            return;
        }
    }


    size_t sz = netcard->rx_buffer[i].command & DC_LENGTH;

    DEVICE_MSG("[RTL8111B/8168B] Empfange %i B von Deskriptor %i\n",
        (int)sz - 4, i);

    if (sz < 5)
    {
        rtl8168b_unown_rx_desc(netcard, i);
        return;
    }

    cdi_net_receive((struct cdi_net_device*) netcard,
        netcard->rx_area[i]->vaddr, sz - 4);

    rtl8168b_unown_rx_desc(netcard, i);
}

static void rtl8168b_handle_transmit_ok(struct rtl8168b_device* netcard)
{
    (void) netcard;
}

static void rtl8168b_handle_link_change(struct rtl8168b_device* netcard)
{
    uint8_t status = read_register_8(netcard, REG_PHY_STATUS);

    if ((status & PS_LINK) != PS_LINK) {
        DEVICE_MSG("[RTL8111B/8168B] Link down\n");
    } else {
        uint16_t speed = (status & PS_FULLDUP) << 15;

        if (status & PS_1000) {
            speed |= 1000;
        } else if (status & PS_100) {
            speed |= 100;
        } else if (status & PS_10) {
            speed |= 10;
        }

        DEVICE_MSG("[RTL8111B/8168B] Link up (%uMb/s %s-duplex)\n",
            speed & 0x7FFF, (speed & 0x8000) ? "Full" : "Half");
    }
}

static void rtl8168b_handle_interrupt(struct cdi_device* device)
{
    struct rtl8168b_device* netcard = (struct rtl8168b_device*) device;
    uint16_t isr = read_register_16(netcard, REG_INTERRUPT_STATUS);

    write_register_16(netcard, REG_INTERRUPT_STATUS, isr);

    DEVICE_MSG("[RTL8111B/8168B] Interrupt: %x\n", isr);

    if (isr & ISR_RECEIVE_OK) {
        rtl8168b_handle_receive_ok(netcard);
    }

    if (isr & ISR_TRANSMIT_OK) {
        rtl8168b_handle_transmit_ok(netcard);
    }

    if (isr & ISR_LINK_CHANGE) {
        rtl8168b_handle_link_change(netcard);
    }
}
