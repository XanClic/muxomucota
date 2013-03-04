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

#include <stdio.h>
#include <string.h>

#include "rtl8168b.h"
#include "cdi/io.h"
#include "cdi/misc.h"

#define DEBUG_MSG(s) printf("[RTL8111B/8168B] debug: %s() '%s'\n", __FUNCTION__, s)
#define DEVICE_MSG printf

static void rtl8168b_handle_interrupt(struct cdi_device* device);

static inline void write_register_byte(struct rtl8168b_device* netcard,
    uint8_t reg, uint8_t value)
{
    cdi_outb(netcard->port_base + reg, value);
}

static inline void write_register_word(struct rtl8168b_device* netcard,
    uint8_t reg, uint16_t value)
{
    cdi_outw(netcard->port_base + reg, value);
}

static inline void write_register_dword(struct rtl8168b_device* netcard,
    uint8_t reg, uint32_t value)
{
    cdi_outl(netcard->port_base + reg, value);
}

static inline void write_register_qword(struct rtl8168b_device* netcard,
    uint8_t reg, uint64_t value)
{
    cdi_outl(netcard->port_base + reg, value & 0xFFFFFFFF);
    cdi_outl(netcard->port_base + reg + 4, value >> 32);
}

static inline uint8_t read_register_byte(struct rtl8168b_device* netcard,
    uint8_t reg)
{
    return cdi_inb(netcard->port_base + reg);
}

static inline uint16_t read_register_word(struct rtl8168b_device* netcard,
    uint8_t reg)
{
    return cdi_inw(netcard->port_base + reg);
}

static inline uint32_t read_register_dword(struct rtl8168b_device* netcard,
    uint8_t reg)
{
    return cdi_inl(netcard->port_base + reg);
}

static inline uint64_t read_register_qword(struct rtl8168b_device* netcard,
    uint8_t reg)
{
    return cdi_inl(netcard->port_base + reg)
        | (((uint64_t) cdi_inl(netcard->port_base + reg + 4)) << 32);;
}

struct cdi_device *rtl8168b_init_device(struct cdi_bus_data *bus_data)
{
    struct cdi_pci_device* pci = (struct cdi_pci_device*) bus_data;
    struct rtl8168b_device* netcard;
    struct cdi_mem_area* buf;

    if (!((pci->vendor_id == 0x10ec) && (pci->device_id == 0x8168))) {
        return NULL;
    }

    buf = cdi_mem_alloc(sizeof(*netcard),
        CDI_MEM_PHYS_CONTIGUOUS | CDI_MEM_DMA_4G | 2);

    if (buf == NULL) {
        return NULL;
    }

    netcard = buf->vaddr;
    
    memset(netcard, 0, sizeof(*netcard));

    netcard->phys = buf->paddr.items[0].start;
    netcard->net.dev.bus_data = (struct cdi_bus_data*) pci;

    DEBUG_MSG("Interrupthandler und Ports registrieren");
    cdi_register_irq(pci->irq, rtl8168b_handle_interrupt, &netcard->net.dev);
    cdi_pci_alloc_ioports(pci);

    cdi_list_t reslist = pci->resources;

    struct cdi_pci_resource* res;
    int i;

    for (i = 0; (res = cdi_list_get(reslist, i)); i++) {
        if (res->type == CDI_PCI_IOPORTS) {
            netcard->port_base = res->start;
        }
    }


    DEBUG_MSG("Reset der Karte");
    write_register_byte(netcard, REG_COMMAND, CR_RESET);

    while ((read_register_byte(netcard, REG_COMMAND) & CR_RESET) == CR_RESET);

    DEBUG_MSG("MAC-Adresse auslesen");
    netcard->net.mac =
        read_register_qword(netcard, REG_ID0) & 0xffffffffffffLL;

    DEBUG_MSG("Aktiviere Rx/Tx");
    write_register_byte(netcard, REG_COMMAND,
        CR_RECEIVER_ENABLE | CR_TRANSMITTER_ENABLE);

    DEBUG_MSG("Initialisiere RCR/TCR");
    write_register_dword(netcard, REG_RECEIVE_CONFIGURATION,
        RCR_MXDMA_UNLIMITED | RCR_ACCEPT_BROADCAST | RCR_ACCEPT_PHYS_MATCH);
    write_register_dword(netcard, REG_TRANSMIT_CONFIGURATION,
        TCR_MXDMA_UNLIMITED | TCR_IFG_STANDARD);

    DEBUG_MSG("Setze Interruptmaske");
    write_register_word(netcard, REG_INTERRUPT_STATUS, 0x4fff);
    write_register_word(netcard, REG_INTERRUPT_MASK, 0x0025);

    DEBUG_MSG("Initialisiere Buffer");
    write_register_qword(netcard, REG_RECEIVE_DESCRIPTORS, (uintptr_t) netcard->rx_buffer);
    write_register_qword(netcard, REG_TRANSMIT_DESCRIPTORS, (uintptr_t) netcard->tx_buffer);

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
        write_register_byte(netcard, REG_COMMAND,
            read_register_byte(netcard, REG_COMMAND) & ~CR_RECEIVER_ENABLE);
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
        write_register_byte(netcard, REG_COMMAND,
            read_register_byte(netcard, REG_COMMAND) & ~CR_TRANSMITTER_ENABLE);
    }

    cdi_net_device_init(&netcard->net);

    DEBUG_MSG("Fertig initialisiert");
    return &netcard->net.dev;
}

void rtl8168b_remove_device(struct cdi_device* device)
{
    struct rtl8168b_device* netcard = (struct rtl8168b_device*) device;
    int i;

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

    if (size > (TX_BUFFER_COUNT * TX_BUFFER_SIZE)) {
        DEVICE_MSG("[RTL8111B/8168B] Paket zu groß!\n");

        return;
    }

    if (1 || !__sync_lock_test_and_set(&netcard->lock, 1)) {
        netcard->lock = 1;
    } else {
        DEBUG_MSG("Tx-Buffer ist schon besetzt");
        return;
    }

    DEBUG_MSG("Tx-Buffer reserviert");

    int i;

    for (i = 0; i < TX_BUFFER_COUNT; i++) {
        if ((!(netcard->tx_buffer[i].command & DC_OWN))
            && (!(netcard->tx_buffer[i].command & (DC_FS | DC_LS))))
        {
            break;
        }
    }

    if (i >= TX_BUFFER_COUNT) {
        DEVICE_MSG("[RTL8111B/8168B] Alle Buffer voll!\n");

        return;
    }

    size_t n;

    for (n = 0; (n < size) && (i < TX_BUFFER_COUNT); i++) {
        if ((netcard->tx_buffer[i].command & DC_OWN)
            || (netcard->tx_buffer[i].command & (DC_FS | DC_LS)))
        {
            break;
        }

        if (n == 0) {
            netcard->tx_buffer[i].command = DC_FS;
        }

        if ((size - n) > TX_BUFFER_SIZE) {
            memcpy(netcard->tx_area[i]->vaddr, &((uint8_t*) data)[n], TX_BUFFER_SIZE);

            netcard->tx_buffer[i].command |= TX_BUFFER_SIZE;
            n += TX_BUFFER_SIZE;
        } else {
            memcpy(netcard->tx_area[i]->vaddr, &((uint8_t*) data)[n], size - n);

            netcard->tx_buffer[i].command |= size - n;
            n += size - n;
        }

        if (n == size) {
            netcard->tx_buffer[i].command |= DC_LS;
        }
    }

    if (i >= TX_BUFFER_COUNT) {
        i--;

        netcard->tx_buffer[i].command |= DC_EOR;
    }

    if (!(netcard->tx_buffer[i].command & DC_LS)) {
        DEVICE_MSG("[RTL8111B/8168B] Nicht genügend freie Buffer!\n");

        for (; i > 0; i--) {
            netcard->tx_buffer[i].command &= DC_EOR;
        }
    } else {
        write_register_byte(netcard, REG_TRANSMIT_POLL, TPR_PACKET_WAITING);
    } 
}

static void rtl8168b_handle_receive_ok(struct rtl8168b_device* netcard)
{
    int i;
    size_t n = 0;

    for (i = 0; i < RX_BUFFER_COUNT; i++) {
        if ((!(netcard->rx_buffer[i].command & DC_OWN))
            && (netcard->rx_buffer[i].command & DC_LS))
        {
            n = netcard->rx_buffer[i].command & DC_LENGTH;
            
            break;
        }
    }

    if (n != 0) {
        uint8_t data[n - 4];

        for (; i >= 0; i--) {
            memcpy(&data[i * RX_BUFFER_SIZE], netcard->rx_area[i]->vaddr,
                (netcard->rx_buffer[i].command & DC_LS) ?
                (n - 4) % RX_BUFFER_SIZE : RX_BUFFER_SIZE);
        }

        cdi_net_receive((struct cdi_net_device* )netcard, data, n - 4);
    }

    for (i = 0; i < RX_BUFFER_COUNT; i++) {
        if (!(netcard->rx_buffer[i].command & DC_OWN)) {
            netcard->rx_buffer[i].command = DC_OWN | RX_BUFFER_SIZE;
        }

        if (i == (RX_BUFFER_COUNT - 1)) {
            netcard->rx_buffer[i].command |= DC_EOR;
        }
    }
}

static void rtl8168b_handle_transmit_ok(struct rtl8168b_device* netcard)
{
    int i;

    for (i = 0; i < TX_BUFFER_COUNT; i++) {
        if ((!(netcard->tx_buffer[i].command & DC_OWN))
            && (netcard->tx_buffer[i].command & DC_FS))
        {
            break;
        }
    }

    for (; i < TX_BUFFER_COUNT; i++) {
        if (netcard->tx_buffer[i].command & DC_LS) {
            netcard->tx_buffer[i].command &= DC_EOR;

            break;
        }

        netcard->tx_buffer[i].command &= DC_EOR;
    }
}

static void rtl8168b_handle_link_change(struct rtl8168b_device* netcard)
{
    uint8_t status = read_register_byte(netcard, REG_PHY_STATUS);

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

        DEVICE_MSG("[RTL8111B/8168B] Link up (%uMb/s %s-duplex)\n", speed & 0x7FFF, (speed & 0x8000) ? "Full" : "Half");
    }
}

static void rtl8168b_handle_interrupt(struct cdi_device* device)
{
    DEBUG_MSG("[RTL8111B/8168B] Interrupt>");
    struct rtl8168b_device* netcard = (struct rtl8168b_device*) device;
    uint16_t isr = read_register_word(netcard, REG_INTERRUPT_STATUS);
    uint16_t clear = 0;

    if (isr & ISR_RECEIVE_OK) {
        rtl8168b_handle_receive_ok(netcard);

        clear |= ISR_RECEIVE_OK;
    }

    if (isr & ISR_TRANSMIT_OK) {
        rtl8168b_handle_transmit_ok(netcard);

        clear |= ISR_TRANSMIT_OK;
    }

    if (isr & ISR_LINK_CHANGE) {
        rtl8168b_handle_link_change(netcard);

        clear |= ISR_LINK_CHANGE;
    }

    write_register_word(netcard, REG_INTERRUPT_STATUS, clear);
}
