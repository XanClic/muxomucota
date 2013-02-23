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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cdi/net.h"
#include "cdi/pci.h"
#include "cdi/io.h"
#include "cdi/misc.h"
#include "cdi/mem.h"

#include "ne2k.h"
#include "ethernet.h"

//Hier koennen die Debug-Nachrichten aktiviert werden
// #define DEBUG_MSG(s) printf("[ne2k] debug: %s() '%s'\n", __FUNCTION__, s)
#define DEBUG_MSG(s) //

static void ne2k_handle_interrupt(struct cdi_device* device);
static void receive_ok_handler(struct ne2k_device* netcard);
static void transmit_ok_handler(struct ne2k_device* netcard);

static inline void write_register_byte(struct ne2k_device* netcard, uint8_t reg, uint8_t value)
{
    cdi_outb(netcard->port_base + reg, value);
}

static inline void write_register_word(struct ne2k_device* netcard, uint8_t reg, uint16_t value)
{
    cdi_outw(netcard->port_base + reg, value);
}

static inline void write_register_dword(struct ne2k_device* netcard, uint8_t reg, uint32_t value)
{
    cdi_outl(netcard->port_base + reg, value);
}

static inline uint8_t read_register_byte(struct ne2k_device* netcard, uint8_t reg)
{
    return cdi_inb(netcard->port_base + reg);
}

static inline uint16_t read_register_word(struct ne2k_device* netcard, uint8_t reg)
{
    return cdi_inw(netcard->port_base + reg);
}

static inline uint32_t read_register_dword(struct ne2k_device* netcard, uint8_t reg)
{
    return cdi_inl(netcard->port_base + reg);
}

struct cdi_device* ne2k_init_device(struct cdi_bus_data* bus_data)
{
    struct cdi_pci_device* pci = (struct cdi_pci_device*) bus_data;
    struct ne2k_device* netcard;
    struct cdi_mem_area* buf;

    if (!((pci->vendor_id == 0x10ec) && (pci->device_id == 0x8029))) {
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

    // PCI-bezogenes Zeug initialisieren
    DEBUG_MSG("Interrupthandler und Ports registrieren");
    cdi_register_irq(pci->irq, ne2k_handle_interrupt, &netcard->net.dev);
    cdi_pci_alloc_ioports(pci);

    cdi_list_t reslist = pci->resources;
    struct cdi_pci_resource* res;
    int i;
    for (i = 0; (res = cdi_list_get(reslist, i)); i++) {
        if (res->type == CDI_PCI_IOPORTS) {
            netcard->port_base = res->start;
        }
    }

    // Reset the card
    DEBUG_MSG("Resetting the card");
    write_register_byte(netcard, NE_CMD, 0x21);

    // Enable 16-bit transfer, turn on monitor mode to avoid receiving packets
    // and turn on loopback just in case. Receive logic is not ready yet.
    write_register_byte(netcard, NE_DCR, 0x09);
    write_register_byte(netcard, NE_RCR, 0x20);
    write_register_byte(netcard, NE_TCR, 0x02);

    // Disable card interrupts
    write_register_byte(netcard, NE_ISR, 0xFF);
    write_register_byte(netcard, NE_IMR, 0);

    // Read the MAC address from PROM
    write_register_byte(netcard, NE_RSAR0, 0);
    write_register_byte(netcard, NE_RSAR1, 0);

    write_register_byte(netcard, NE_RBCR0, 32);
    write_register_byte(netcard, NE_RBCR1, 0);

    write_register_byte(netcard, NE_CMD, 0x0A);

    uint16_t prom[16];
    uint8_t mac[6];
    for(i = 0; i < 16; i++) {
        prom[i] = read_register_word(netcard, NE_DATA);
    }

    // Set the MAC address for the system use
    for(i = 0; i < 6; i++) {
        mac[i] = (uint8_t) (prom[i] & 0xFF);
    }
    netcard->net.mac = (*(uint32_t*) (&mac[0])) | ((uint64_t) ((*(uint32_t*) (&mac[4])) & 0xFFFF) << 32);

    // Setup the packet ring buffer
    write_register_byte(netcard, NE_CMD, 0x61);
    netcard->next_packet = PAGE_RX + 1;
    write_register_byte(netcard, NE_CURR, netcard->next_packet);

    write_register_byte(netcard, NE_CMD, 0x21);

    write_register_byte(netcard, NE_PSTART, PAGE_RX);
    write_register_byte(netcard, NE_BNDRY, PAGE_RX);
    write_register_byte(netcard, NE_PSTOP, PAGE_STOP);

    // Accept broadcast and runt packets
    write_register_byte(netcard, NE_RCR, 0x06);
    write_register_byte(netcard, NE_TCR, 0);

    // Clear pending interrupts, enable them all, and begin card operation
    write_register_byte(netcard, NE_ISR, 0xFF);
    write_register_byte(netcard, NE_IMR, 0x3F);
    write_register_byte(netcard, NE_CMD, 0x22);

    netcard->pending_sends = cdi_list_create();

    cdi_net_device_init(&netcard->net);
    DEBUG_MSG("Fertig initialisiert");

    return &netcard->net.dev;
}

void ne2k_remove_device(struct cdi_device* device)
{
}

void ne2k_send_packet
    (struct cdi_net_device* device, void* data, size_t size)
{
    struct ne2k_device* netcard = (struct ne2k_device*) device;

    if (size > 0x700) {
        // Spezialfall - keine Lust
        printf("ne2k: size ist boese\n");
        return;
    }

    if (!__sync_lock_test_and_set(&netcard->tx_in_progress, 1)) {
        netcard->tx_in_progress = 1;
    } else {
        DEBUG_MSG("Tx-Buffer ist schon besetzt");

        void* pending = malloc(size + sizeof(uint32_t));
        cdi_list_insert(netcard->pending_sends,
            cdi_list_size(netcard->pending_sends), pending);
        *((uint32_t*) pending) = size;
        pending += sizeof(uint32_t);
        memcpy(pending, data, size);

        return;
    }

    // Send the length/address for this write
    write_register_byte(netcard, NE_RSAR0, 0);
    write_register_byte(netcard, NE_RSAR1, PAGE_TX);

    write_register_byte(netcard, NE_RBCR0, (size > 64) ? (size & 0xff) : 64);
    write_register_byte(netcard, NE_RBCR1, size >> 8);

    write_register_byte(netcard, NE_CMD, 0x12);

    // Write to the NIC
    uint16_t *p = (uint16_t*) data;
    size_t i;
    for(i = 0; (i + 1) < size; i += 2) {
        write_register_word(netcard, NE_DATA, p[i/2]);
    }

    // Handle odd bytes
    if(size & 1) {
        write_register_byte(netcard, NE_DATA, p[i/2]);
        i++;
    }

    // Pad to 64 bytes, if needed
    for(; i < 64; i += 2) {
        write_register_word(netcard, NE_DATA, 0);
    }

    // Await the transfer completion and then transmit
    // FIXME: Should wait upon a mutex or something which is unlocked when
    // the IRQ fires.
    while(!(read_register_byte(netcard, NE_ISR) & 0x40));
    write_register_byte(netcard, NE_ISR, 0x40);

    write_register_byte(netcard, NE_TBCR0, (size > 64) ? (size & 0xff) : 64);
    write_register_byte(netcard, NE_TBCR1, size >> 8);

    write_register_byte(netcard, NE_TPSR, PAGE_TX);

    write_register_byte(netcard, NE_CMD, 0x26);
}

static void receive_ok_handler(struct ne2k_device* netcard)
{
    DEBUG_MSG("ROK");

    write_register_byte(netcard, NE_ISR, 0x1);

    write_register_byte(netcard, NE_CMD, 0x61);
    uint8_t current = read_register_byte(netcard, NE_CURR);
    write_register_byte(netcard, NE_CMD, 0x21);

    while (netcard->next_packet != current)
    {
        write_register_byte(netcard, NE_RSAR0, 0);
        write_register_byte(netcard, NE_RSAR1, netcard->next_packet);

        // 4 bytes - 2 for status, 2 for length
        write_register_byte(netcard, NE_RBCR0, 4);
        write_register_byte(netcard, NE_RBCR1, 0);

        write_register_byte(netcard, NE_CMD, 0x0A);

        uint16_t status = read_register_word(netcard, NE_DATA);
        uint16_t length = read_register_word(netcard, NE_DATA);

        if(!length) {
            break;
        }

        // Remove status and length bytes
        length -= 3;

        if (length >= sizeof(struct eth_packet_header))
        {
            // Verify status
            while(!(read_register_byte(netcard, NE_ISR) & 0x40));
            write_register_byte(netcard, NE_ISR, 0x40);

            // Read the packet
            write_register_byte(netcard, NE_RSAR0, 4);
            write_register_byte(netcard, NE_RSAR1, netcard->next_packet);
            write_register_byte(netcard, NE_RBCR0, length & 0xFF);
            write_register_byte(netcard, NE_RBCR1, (length >> 8) & 0xFF);

            write_register_byte(netcard, NE_CMD, 0x0A);

            uint16_t data[(length / 2) + 1];
            int i, words = (length / 2);
            for(i = 0; i < words; ++i) {
                data[i] = read_register_word(netcard, NE_DATA);
            }
            if(length & 1) {
                data[i] = read_register_word(netcard, NE_DATA) & 0xFF;
            }

            // Verify status
            while(!(read_register_byte(netcard, NE_ISR) & 0x40));
            write_register_byte(netcard, NE_ISR, 0x40);

            netcard->next_packet = status >> 8;
            write_register_byte(netcard, NE_BNDRY, (netcard->next_packet == PAGE_RX) ? (PAGE_STOP - 1) : (netcard->next_packet - 1));

            cdi_net_receive(
                (struct cdi_net_device*) netcard,
                (uint8_t*) data,
                length);
        }
    }
}

static void transmit_ok_handler(struct ne2k_device* netcard)
{
    DEBUG_MSG("TOK");
    netcard->tx_in_progress = 0;
}

static void ne2k_handle_interrupt(struct cdi_device* device)
{
    DEBUG_MSG("<ne2k Interrupt>");

    struct ne2k_device* netcard = (struct ne2k_device*) device;
    uint32_t isr = read_register_word(netcard, NE_ISR);

    // Packet received?
    if(isr & 0x05) {
        DEBUG_MSG("ne2k: received packet");
        if(!(isr & 0x04)) {
            write_register_byte(netcard, NE_IMR, 0x3A);
            isr &= ~1;
            receive_ok_handler(netcard);
            write_register_byte(netcard, NE_IMR, 0x3F);
        }
        else {
            DEBUG_MSG("ne2k: receive failed");
        }
    }

    // Packet transmitted?
    if(isr & 0x0A) {
        if(!(isr & 0x8)) {
            transmit_ok_handler(netcard);
        }
    }

    // Overflows
    if(isr & 0x10) {
        DEBUG_MSG("ne2K: receive buffer overflow");
    }
    if(isr & 0x20) {
        DEBUG_MSG("ne2K: counter overflow");
    }

    write_register_byte(netcard, NE_ISR, isr);

    // Falls noch Pakete zu senden waren, die nicht gesendet werden
    // konnten, weil die Karte beschaeftigt war, das jetzt nachholen
    void* pending = cdi_list_pop(netcard->pending_sends);
    if (pending) {
        uint32_t size = *((uint32_t*) pending);
        pending += sizeof(uint32_t);
        DEBUG_MSG("Sende Paket aus der pending-Queue");
        ne2k_send_packet((struct cdi_net_device*) device, pending, size);
        free(pending - sizeof(uint32_t));
    }

    DEBUG_MSG("interrupt returns");
}
