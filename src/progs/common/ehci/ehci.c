/******************************************************************************
 * Copyright (c) 2015 Max Reitz                                               *
 *                                                                            *
 * Permission is hereby granted,  free of charge,  to any person  obtaining a *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction,  including without limitation *
 * the rights to use, copy,  modify, merge, publish,  distribute, sublicense, *
 * and/or  sell copies of the  Software,  and to permit  persons to whom  the *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED,  INCLUDING BUT NOT LIMITED TO THE WARRANTIES  OF MERCHANTABILITY, *
 * FITNESS  FOR A PARTICULAR  PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL *
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
 * LIABILITY,  WHETHER IN AN ACTION OF CONTRACT,  TORT OR OTHERWISE,  ARISING *
 * FROM,  OUT OF  OR IN CONNECTION  WITH  THE SOFTWARE  OR THE  USE OR  OTHER *
 * DEALINGS IN THE SOFTWARE.                                                  *
 ******************************************************************************/

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cdi.h>
#include <cdi/lists.h>
#include <cdi/misc.h>
#include <cdi/pci.h>
#include <cdi/usb.h>
#include <cdi/usb_hcd.h>

#include "ehci.h"


#define EHCI_TIMEOUT 1000 // ms


static bool bios_handoff(struct cdi_pci_device *pci_dev, struct ehci *hc);
static void irq_handler(struct cdi_device *dev);


struct cdi_device *ehci_init_device(struct cdi_bus_data *bus_data)
{
    struct cdi_pci_device *pci_dev = (struct cdi_pci_device *)bus_data;
    static int dev_counter = 0;

    if (pci_dev->class_id     != PCI_CLASS_SERIAL_BUS_CONTROLLER ||
        pci_dev->subclass_id  != PCI_SUBCLASS_USB_HOST_CONTROLLER ||
        pci_dev->interface_id != PCI_INTERFACE_EHCI)
    {
        return NULL;
    }

    struct ehci *hc = calloc(1, sizeof(*hc));

    hc->hc.dev.name = malloc(8);
    sprintf((char *)hc->hc.dev.name, "ehci%i", dev_counter++);

    hc->retired_qhs = cdi_list_create();

    cdi_pci_alloc_memory(pci_dev);

    struct cdi_pci_resource *res;
    for (int i = 0; (res = cdi_list_get(pci_dev->resources, i)); i++) {
        if (res->type == CDI_PCI_MEMORY && !res->index) {
            hc->caps = res->address;
            hc->regs = (struct ehci_registers *)((uintptr_t)res->address +
                                                 hc->caps->caplength);
            break;
        }
    }
    if (!hc->regs) {
        goto fail;
    }

    if (!bios_handoff(pci_dev, hc)) {
        goto fail;
    }

    // Stop operation
    hc->regs->usbcmd = (hc->regs->usbcmd & EHCI_CMD_RSVD) | EHCI_CMD_ITC(8);

    // Wait until HCHALTED becomes true
    CDI_CONDITION_WAIT(hc->regs->usbsts & EHCI_STS_HCH, 50);
    if (!(hc->regs->usbsts & EHCI_STS_HCH)) {
        goto fail;
    }

    hc->regs->usbcmd |= EHCI_CMD_HCRES; // Reset
    CDI_CONDITION_WAIT(!(hc->regs->usbcmd & EHCI_CMD_HCRES), 50);
    if (hc->regs->usbcmd & EHCI_CMD_HCRES) {
        goto fail;
    }

    if (hc->caps->hccparams & EHCI_HCC_64) {
        // 64-bit addressing capability
        hc->regs->ctrldssegment = 0;
    }

    hc->regs->usbintr = 0;
    hc->regs->usbsts  = hc->regs->usbsts;

    hc->periodic_list_mem = cdi_mem_alloc(4096, 12
                                                | CDI_MEM_PHYS_CONTIGUOUS
                                                | CDI_MEM_DMA_4G
                                                | CDI_MEM_NOINIT);
    hc->periodic_list = hc->periodic_list_mem->vaddr;
    for (int i = 0; i < 1024; i++) {
        hc->periodic_list[i] = EHCI_T;
    }
    hc->regs->periodiclistbase = hc->periodic_list_mem->paddr.items[0].start;

    // Create anchor
    struct cdi_mem_area *anchor_qh;
    anchor_qh = cdi_mem_alloc(sizeof(ehci_fat_qh_t), 6
                                                     | CDI_MEM_PHYS_CONTIGUOUS
                                                     | CDI_MEM_DMA_4G
                                                     | CDI_MEM_NOINIT);

    hc->async_start = anchor_qh->vaddr;
    memset(hc->async_start, 0, sizeof(*hc->async_start));

    hc->async_start->cdi_mem_area = anchor_qh;
    hc->async_start->paddr = anchor_qh->paddr.items[0].start
                           + offsetof(ehci_fat_qh_t, qh);

    hc->async_start->next = hc->async_start;
    hc->async_start->previous = hc->async_start;

    hc->async_start->qh.next_qh = hc->async_start->paddr | EHCI_QH;
    hc->async_start->qh.ep_state[0] = EHCI_QHES0_RL(0) | EHCI_QHES0_MPL(8)
                                    | EHCI_QHES0_EP(0) | EHCI_QHES0_DEV(0)
                                    | EHCI_QHES0_H | EHCI_QHES0_DTC
                                    | EHCI_QHES0_EPS_HS;
    hc->async_start->qh.ep_state[1] = EHCI_QHES1_MULT(1) | EHCI_QHES1_SCM(0xff);

    hc->async_start->qh.overlay.next_qtd = EHCI_T;
    hc->async_start->qh.overlay.alt_next_qtd = EHCI_T;

    hc->regs->asynclistaddr = hc->async_start->paddr;

    // Start operation
    hc->regs->usbcmd = (hc->regs->usbcmd & EHCI_CMD_RSVD)
                      | EHCI_CMD_ITC(8) | EHCI_CMD_ASE | EHCI_CMD_PSE
                      | EHCI_CMD_FLS_1K | EHCI_CMD_RS;

    // Wait until HCHalted becomes false and the schedules are running
    CDI_CONDITION_WAIT((hc->regs->usbsts
                        & (EHCI_STS_ASS | EHCI_STS_PSS | EHCI_STS_HCH))
                       == (EHCI_STS_ASS | EHCI_STS_PSS), 50);
    if ((hc->regs->usbsts & (EHCI_STS_ASS | EHCI_STS_PSS | EHCI_STS_HCH))
        != (EHCI_STS_ASS | EHCI_STS_PSS))
    {
        goto fail;
    }

    // async advance doorbell
    hc->regs->usbintr = EHCI_INT_IAA;
    cdi_register_irq(pci_dev->irq, &irq_handler, &hc->hc.dev);

    // Route everything to this HC
    hc->regs->configflag |= EHCI_CF_CF;

    cdi_sleep_ms(5);

    hc->hc.rh.ports = EHCI_HCS_N_PORTS(hc->caps->hcsparams);

    // Power up all ports
    for (int i = 0; i < hc->hc.rh.ports; i++) {
        hc->regs->portsc[i] |= EHCI_PSC_PP;
    }

    return &hc->hc.dev;

fail:
    if (hc->async_start) {
        cdi_mem_free(hc->async_start->cdi_mem_area);
    }
    if (hc->periodic_list_mem) {
        cdi_mem_free(hc->periodic_list_mem);
    }
    cdi_list_destroy(hc->retired_qhs);
    free((void *)hc->hc.dev.name);
    free(hc);
    cdi_pci_free_memory(pci_dev);

    return NULL;
}


static bool bios_handoff(struct cdi_pci_device *pci_dev, struct ehci *hc)
{
    int eecp = EHCI_HCC_EECP(hc->caps->hccparams);

    // Iterate through the list of capabilities
    while (eecp >= 0x40) {
        uint32_t cap = cdi_pci_config_readl(pci_dev, eecp);

        // Look for USBLEGSUP
        if (EHCI_EEC_ID(cap) == EHCI_EECID_LEGSUP) {
            // Set OS semaphore
            cdi_pci_config_writeb(pci_dev, eecp + 3, 1);

            // Wait for BIOS semaphore to get unset
            CDI_CONDITION_WAIT(!(cdi_pci_config_readb(pci_dev, eecp + 2) & 1),
                               1000);
            if (cdi_pci_config_readb(pci_dev, eecp + 2) & 1) {
                return false;
            }
        }

        eecp = EHCI_EEC_NEXT(cap);
    }

    return true;
}


static void irq_handler(struct cdi_device *dev)
{
    struct cdi_usb_hc *cdi_hc = CDI_UPCAST(dev, struct cdi_usb_hc, dev);
    struct ehci *hc = CDI_UPCAST(cdi_hc, struct ehci, hc);

    uint32_t usbsts = hc->regs->usbsts;
    hc->regs->usbsts = usbsts;

    // async advance doorbell
    if (!(usbsts & EHCI_STS_IAA)) {
        return;
    }

    ehci_fat_qh_t *rqh;
    while ((rqh = cdi_list_pop(hc->retired_qhs))) {
        cdi_mem_free(rqh->cdi_mem_area);
    }
}


void ehci_rh_port_down(struct cdi_usb_hub *hub, int index)
{
    struct cdi_usb_hc *cdi_hc = CDI_UPCAST(hub, struct cdi_usb_hc, rh);
    struct ehci *hc = CDI_UPCAST(cdi_hc, struct ehci, hc);

    hc->regs->portsc[index] &= ~EHCI_PSC_PED;
}


void ehci_rh_port_up(struct cdi_usb_hub *hub, int index)
{
    struct cdi_usb_hc *cdi_hc = CDI_UPCAST(hub, struct cdi_usb_hc, rh);
    struct ehci *hc = CDI_UPCAST(cdi_hc, struct ehci, hc);
    uint32_t portsc = hc->regs->portsc[index];

    if ((portsc & EHCI_PSC_LS_MASK) == EHCI_PSC_LS_K) {
        // Low-speed device, release ownership
        hc->regs->portsc[index] = portsc | EHCI_PSC_PO;
        return;
    }

    // Assert reset signal
    hc->regs->portsc[index] = ((portsc & ~(EHCI_PSC_PO | EHCI_PSC_SUS |
                                           EHCI_PSC_FPR | EHCI_PSC_PED))
                                       | EHCI_PSC_RES);
    cdi_sleep_ms(50);

    // De-assert reset signal
    hc->regs->portsc[index] = portsc & ~EHCI_PSC_RES;
    CDI_CONDITION_WAIT(!(hc->regs->portsc[index] & EHCI_PSC_RES), 50);

    portsc = hc->regs->portsc[index];
    if (portsc & EHCI_PSC_RES) {
        // Disable port, route on to companion controller
        // (maybe it can do at least something with it)
        hc->regs->portsc[index] = (portsc & ~EHCI_PSC_PED) | EHCI_PSC_PO;
    } else if ((portsc & (EHCI_PSC_PED | EHCI_PSC_CCS)) == EHCI_PSC_CCS) {
        // Device connected but port disabled:
        // Full-speed device, release ownership
        hc->regs->portsc[index] = portsc | EHCI_PSC_PO;
    }
}


cdi_usb_port_status_t ehci_rh_port_status(struct cdi_usb_hub *hub, int index)
{
    struct cdi_usb_hc *cdi_hc = CDI_UPCAST(hub, struct cdi_usb_hc, rh);
    struct ehci *hc = CDI_UPCAST(cdi_hc, struct ehci, hc);
    uint32_t portsc = hc->regs->portsc[index];
    cdi_usb_port_status_t status = 0;

    if ((portsc & (EHCI_PSC_PED | EHCI_PSC_CCS)) ==
                  (EHCI_PSC_PED | EHCI_PSC_CCS))
    {
        // Device connected and port enabled
        status |= CDI_USB_PORT_CONNECTED | CDI_USB_HIGH_SPEED;
    }

    return status;
}


static void enter_async_qh(struct ehci *hc, ehci_fat_qh_t *qh)
{
    // Enter the new QH as last

    qh->next = hc->async_start;
    qh->previous = hc->async_start->previous;

    qh->previous->next = qh;
    hc->async_start->previous = qh;

    qh->qh.next_qh = hc->async_start->paddr | EHCI_QH;
    qh->previous->qh.next_qh = qh->paddr | EHCI_QH;
}


static void retire_qh(struct ehci *hc, ehci_fat_qh_t *qh)
{
    qh->previous->qh.next_qh = qh->qh.next_qh;

    qh->previous->next = qh->next;
    qh->next->previous = qh->previous;

    cdi_list_push(hc->retired_qhs, qh);
    // Request async advance doorbell
    hc->regs->usbcmd |= EHCI_CMD_IAADB;
}


static void retire_qtd(ehci_fat_qtd_t *qtd)
{
    ehci_fat_qh_t *qh = qtd->qh;

    uint32_t err_mask = (qh->qh.ep_state[0] & EHCI_QHES0_EPS_MASK)
                         == EHCI_QHES0_EPS_HS
                        ? EHCI_QTDS_HS_ERR_MASK
                        : EHCI_QTDS_LFS_ERR_MASK;

    if (qtd->qtd.status & err_mask) {
        if (qtd->qtd.status & EHCI_QTDS_BABBLE) {
            *qtd->result = CDI_USB_BABBLE;
        } else if (qtd->qtd.status & (EHCI_QTDS_XACTERR | EHCI_QTDS_P_ERR)) {
            *qtd->result = CDI_USB_BAD_RESPONSE;
        } else if (qtd->qtd.status & (EHCI_QTDS_BUFERR | EHCI_QTDS_MuF)) {
            *qtd->result = CDI_USB_HC_ERROR;
        } else if (qtd->qtd.status & EHCI_QTDS_HALTED) {
            *qtd->result = CDI_USB_STALL;
        } else {
            *qtd->result = CDI_USB_HC_ERROR;
        }
    }

    if (qtd->write_back) {
        size_t rem = qtd->write_back_size;
        for (int i = 0; i < 5 && rem; i++) {
            size_t sz = rem > 4096 ? 4096 : rem;
            memcpy((void *)((uintptr_t)qtd->write_back + i * 4096),
                   qtd->buffers[i]->vaddr, sz);
            rem -= sz;
        }
        assert(!rem);
    }

    qh->first_qtd = qtd->next;
    if (qh->last_qtd == qtd) {
        qh->last_qtd = NULL;
    }

    for (int i = 0; i < 5; i++) {
        if (qtd->buffers[i]) {
            cdi_mem_free(qtd->buffers[i]);
        }
    }

    cdi_mem_free(qtd->cdi_mem_area);
}


static void enter_qtd(ehci_fat_qh_t *qh, ehci_fat_qtd_t *qtd)
{
    bool first;

    qtd->qh = qh;
    qtd->qtd.next_qtd = EHCI_T;
    qtd->qtd.alt_next_qtd = EHCI_T;

    while (qh->first_qtd) {
        // Break if an active qTD is encountered
        if (qh->first_qtd->qtd.status & EHCI_QTDS_ACTIVE) {
            break;
        }

        retire_qtd(qh->first_qtd);
    }

    first = !qh->first_qtd;

    // Note: Race condition (a qTD might retire here and we are thus appending
    // the one to be queued to an inactive qTD); it's fine as long as we are
    // constantly rescanning the queue (here and in ehci_wait_transaction())

    if (first) {
        qh->first_qtd = qtd;
    } else {
        qh->last_qtd->next = qtd;
    }
    qh->last_qtd = qtd;

    if (!first) {
        qh->last_qtd->qtd.next_qtd = qtd->paddr;
    }

    // If the QH is not active, it's done with its queue of qTDs and we should
    // continue from this qTD
    if (!(qh->qh.overlay.status & EHCI_QTDS_ACTIVE)) {
        qh->qh.overlay.next_qtd = qh->first_qtd->paddr;
    }
}


cdi_usb_hc_transaction_t ehci_create_transaction(struct cdi_usb_hc *cdi_hc,
    const struct cdi_usb_hc_ep_info *target)
{
    struct ehci *hc = CDI_UPCAST(cdi_hc, struct ehci, hc);
    struct cdi_mem_area *qh_mem;

    qh_mem = cdi_mem_alloc(sizeof(ehci_fat_qh_t), 6
                                                  | CDI_MEM_PHYS_CONTIGUOUS
                                                  | CDI_MEM_DMA_4G
                                                  | CDI_MEM_NOINIT);
    ehci_fat_qh_t *qh = qh_mem->vaddr;

    memset(qh, 0, sizeof(*qh));

    int non_high_speed_control = target->speed != CDI_USB_HIGH_SPEED
                                 && target->ep_type == CDI_USB_CONTROL;

    qh->qh.ep_state[0] = EHCI_QHES0_RL(0)
                       | (non_high_speed_control ? EHCI_QHES0_C : 0)
                       | EHCI_QHES0_MPL(target->mps)
                       | EHCI_QHES0_DTC
                       | EHCI_QHES0_EP(target->ep)
                       | EHCI_QHES0_DEV(target->dev);
    switch (target->speed) {
        case CDI_USB_LOW_SPEED:  qh->qh.ep_state[0] |= EHCI_QHES0_EPS_LS; break;
        case CDI_USB_FULL_SPEED: qh->qh.ep_state[0] |= EHCI_QHES0_EPS_FS; break;
        case CDI_USB_HIGH_SPEED: qh->qh.ep_state[0] |= EHCI_QHES0_EPS_HS; break;
        case CDI_USB_SUPERSPEED:
        default:
            abort();
    }

    qh->qh.ep_state[1] = EHCI_QHES1_MULT(1)
                       | EHCI_QHES1_PORT(target->tt_port + 1)
                       | EHCI_QHES1_HUB(target->tt_addr)
                       | EHCI_QHES1_SCM(0xff);

    qh->qh.overlay.next_qtd = EHCI_T;
    qh->qh.overlay.alt_next_qtd = EHCI_T;

    qh->cdi_mem_area = qh_mem;
    qh->paddr = qh_mem->paddr.items[0].start + offsetof(ehci_fat_qh_t, qh);

    enter_async_qh(hc, qh);

    return qh;
}


static void create_qtd(ehci_fat_qh_t *qh, cdi_usb_transfer_token_t token,
                       int toggle, void *buffer, size_t size,
                       cdi_usb_transmission_result_t *result)
{
    struct cdi_mem_area *qtd_mem;

    qtd_mem = cdi_mem_alloc(sizeof(ehci_fat_qtd_t), 6
                                                    | CDI_MEM_PHYS_CONTIGUOUS
                                                    | CDI_MEM_DMA_4G
                                                    | CDI_MEM_NOINIT);
    ehci_fat_qtd_t *qtd = qtd_mem->vaddr;

    memset(qtd, 0, sizeof(*qtd));

    qtd->qtd.status = EHCI_QTDS_DT(toggle)
                    | EHCI_QTDS_XFLEN(size)
                    | EHCI_QTDS_CP(0)
                    | EHCI_QTDS_CERR(3)
                    | EHCI_QTDS_ACTIVE;

    switch (token) {
        case CDI_USB_IN:    qtd->qtd.status |= EHCI_QTDS_PID_IN;    break;
        case CDI_USB_OUT:   qtd->qtd.status |= EHCI_QTDS_PID_OUT;   break;
        case CDI_USB_SETUP: qtd->qtd.status |= EHCI_QTDS_PID_SETUP; break;
        default:
            abort();
    }

    if (token == CDI_USB_IN) {
        qtd->write_back = buffer;
        qtd->write_back_size = size;
    }

    for (int i = 0; i < 5 && size; i++) {
        size_t sz = size > 4096 ? 4096 : size;

        qtd->buffers[i] = cdi_mem_alloc(4096, 12
                                              | CDI_MEM_PHYS_CONTIGUOUS
                                              | CDI_MEM_DMA_4G
                                              | CDI_MEM_NOINIT);
        qtd->qtd.buffers[i] = qtd->buffers[i]->paddr.items[0].start;

        if (token == CDI_USB_OUT || token == CDI_USB_SETUP) {
            memcpy(qtd->buffers[i]->vaddr,
                   (void *)((uintptr_t)buffer + i * 4096), sz);
        }

        size -= sz;
    }

    qtd->cdi_mem_area = qtd_mem;
    qtd->paddr = qtd_mem->paddr.items[0].start + offsetof(ehci_fat_qtd_t, qtd);

    qtd->result = result;

    enter_qtd(qh, qtd);
}


void ehci_enqueue(struct cdi_usb_hc *cdi_hc,
                  const struct cdi_usb_hc_transmission *trans)
{
    (void)cdi_hc;

    ehci_fat_qh_t *qh = trans->ta;
    bool first = true;
    size_t sz = trans->size;
    char *buf = trans->buffer;
    int toggle = trans->toggle;
    size_t mps = EHCI_QHES0_READ_MPL(qh->qh.ep_state[0]);

    *trans->result = CDI_USB_OK;

    while (sz || first) {
        first = false;

        size_t iter_sz = sz > 4096 * 5 ? 4096 * 5 : sz;

        create_qtd(qh, trans->token, toggle, buf, iter_sz, trans->result);

        buf += iter_sz;
        sz -= iter_sz;

        toggle ^= ((iter_sz + mps - 1) / mps) & 1;
    }
}


void ehci_wait_transaction(struct cdi_usb_hc *cdi_hc,
                           cdi_usb_hc_transaction_t ta)
{
    (void)cdi_hc;

    ehci_fat_qh_t *qh = ta;
    bool had_timeout = false;

    irq_handler(&cdi_hc->dev);

    while (qh->first_qtd) {
        if (!had_timeout) {
            if (!(qh->qh.overlay.status & EHCI_QTDS_ACTIVE)) {
                qh->qh.overlay.next_qtd = qh->first_qtd->paddr;
            }

            uint64_t timestamp = cdi_elapsed_ms();

            while ((qh->first_qtd->qtd.status & EHCI_QTDS_ACTIVE) &&
                   cdi_elapsed_ms() - timestamp < EHCI_TIMEOUT)
            {
                // I don't really know who's to blame for this (qemu or
                // týndur), and actually, I don't even want to know.
                // As a matter of fact, qemu will not work on the async
                // schedule if the async advance doorbell is set but the driver
                // has not acknowledged this; however, the interrupt handler
                // will always clear that bit and the interrupt handler is
                // active all the time, so something is apparently sometimes
                // blocking the handler from being executed. Force it here to
                // prevent qemu's HC from stalling.
                irq_handler(&cdi_hc->dev);
            }

            had_timeout = qh->first_qtd->qtd.status & EHCI_QTDS_ACTIVE;
        }

        if (had_timeout) {
            // Stop this queue
            qh->qh.overlay.next_qtd = EHCI_T;
            qh->qh.overlay.status &= ~(uint32_t)EHCI_QTDS_ACTIVE;

            *qh->first_qtd->result = CDI_USB_TIMEOUT;
        }

        retire_qtd(qh->first_qtd);
    }
}


void ehci_destroy_transaction(struct cdi_usb_hc *cdi_hc,
                              cdi_usb_hc_transaction_t ta)
{
    struct ehci *hc = CDI_UPCAST(cdi_hc, struct ehci, hc);
    ehci_fat_qh_t *qh = ta;

    qh->qh.overlay.next_qtd = EHCI_T;

    uint64_t timestamp = cdi_elapsed_ms();

    while ((qh->qh.overlay.status & EHCI_QTDS_ACTIVE) &&
           cdi_elapsed_ms() - timestamp < EHCI_TIMEOUT)
    {
        // See above.
        irq_handler(&cdi_hc->dev);
    }

    if (!(qh->qh.overlay.status & EHCI_QTDS_ACTIVE)) {
        qh->qh.overlay.status &= ~(uint32_t)EHCI_QTDS_ACTIVE;
        // Could probably be solved more elegantly, but we are in a timeout
        // already, so waiting a bit longer will not be that big of a problem
        // (the sleep is here to ensure that the HC is no longer trying to work
        // on the qTDs belonging to this QH)
        cdi_sleep_ms(100);
    }

    while (qh->first_qtd) {
        retire_qtd(qh->first_qtd);
    }
    retire_qh(hc, qh);
}
