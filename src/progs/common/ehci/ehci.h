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

#ifndef EHCI_H
#define EHCI_H

#include <stddef.h>
#include <stdint.h>

#include <cdi/lists.h>
#include <cdi/mem.h>
#include <cdi/usb.h>
#include <cdi/usb_hcd.h>


struct ehci_registers {
    uint32_t usbcmd;
    uint32_t usbsts;
    uint32_t usbintr;
    uint32_t frindex;
    uint32_t ctrldssegment;
    uint32_t periodiclistbase;
    uint32_t asynclistaddr;
    uint8_t reserved[0x24];
    uint32_t configflag;
    uint32_t portsc[];
} __attribute__((packed));

struct ehci_capabilities {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hciversion;
    uint32_t hcsparams;
    uint32_t hccparams;
    uint64_t hcsp_portroute;
} __attribute__((packed));

struct ehci_qtd {
    uint32_t next_qtd;
    uint32_t alt_next_qtd;
    uint32_t status;
    uint32_t buffers[5];

    // 64-bit structures
    uint32_t ext_buffer[5];
} __attribute__((packed));

struct ehci_qh {
    uint32_t next_qh;
    uint32_t ep_state[2];
    uint32_t current_qtd;
    struct ehci_qtd overlay;
} __attribute__((packed));

typedef struct ehci_fat_qh ehci_fat_qh_t;
typedef struct ehci_fat_qtd ehci_fat_qtd_t;

#define DIFF_TO_MULTIPLE(value, divisor) \
    ((divisor) - ((((value) + (divisor) - 1) % (divisor)) + 1))

struct ehci_fat_qh {
    volatile struct ehci_qh qh;

    // Align the QH to 128 bytes (something with cache lines?). Otherwise,
    // there may be conflicts between data we are writing to our very own
    // fields provided by ehci_fat_qh_t and writes done by the HC to this
    // structure.
    uint8_t alignment[DIFF_TO_MULTIPLE(sizeof(struct ehci_qh), 128)];

    struct cdi_mem_area *cdi_mem_area;
    uintptr_t paddr;

    ehci_fat_qh_t *next, *previous;
    ehci_fat_qtd_t *first_qtd, *last_qtd;
};

struct ehci_fat_qtd {
    volatile struct ehci_qtd qtd;

    // Align the qTD to a multiple of 128 bytes.
    uint8_t alignment[DIFF_TO_MULTIPLE(sizeof(struct ehci_qtd), 128)];

    struct cdi_mem_area *cdi_mem_area;
    uintptr_t paddr;

    ehci_fat_qh_t *qh;
    ehci_fat_qtd_t *next;

    struct cdi_mem_area *buffers[5];

    void *write_back;
    size_t write_back_size;

    cdi_usb_transmission_result_t *result;
};

struct ehci {
    struct cdi_usb_hc hc;

    volatile struct ehci_capabilities *caps;
    volatile struct ehci_registers *regs;

    struct cdi_mem_area *periodic_list_mem;
    volatile uint32_t *periodic_list;

    ehci_fat_qh_t *async_start;
    cdi_list_t retired_qhs;
};

#define PCI_CLASS_SERIAL_BUS_CONTROLLER     0x0c
#define PCI_SUBCLASS_USB_HOST_CONTROLLER    0x03
#define PCI_INTERFACE_EHCI                  0x20

#define EHCI_T       (1u << 0)
#define EHCI_ITD     (0u << 1)
#define EHCI_QH      (1u << 1)
#define EHCI_SITD    (2u << 1)
#define EHCI_FSTN    (3u << 1)

// Operational Registers: USBCMD
#define EHCI_CMD_RSVD   0xff00ff00u // Consider async sched park mode reserved
#define EHCI_CMD_ITC(x) ((x) << 16) // Interrupt Threshold Control
#define EHCI_CMD_LHCRES (1 <<  7)   // Light Host Controller Reset
#define EHCI_CMD_IAADB  (1 <<  6)   // Interrupt on Async Advance Doorbell
#define EHCI_CMD_ASE    (1 <<  5)   // Async Schedule Enable
#define EHCI_CMD_PSE    (1 <<  4)   // Periodic Schedule Enable
#define EHCI_CMD_FLS_1K (0 <<  2)   // Frame List Size: 1024 elements (default)
#define EHCI_CMD_HCRES  (1 <<  1)   // Host Controller Reset
#define EHCI_CMD_RS     (1 <<  0)   // Run/Stop (1 = Run)

// Operational Registers: USBINTR
#define EHCI_INT_IAA    (1 <<  5)   // Interrupt on Async Advance Enable
#define EHCI_INT_HSE    (1 <<  4)   // Host System Error Enable
#define EHCI_INT_FLR    (1 <<  3)   // Frame List Rollover Enable
#define EHCI_INT_PCD    (1 <<  2)   // Port Change Detect Enable
#define EHCI_INT_USBERR (1 <<  1)   // USB Error Interrupt
#define EHCI_INT_USBINT (1 <<  0)   // USB Interrupt

// Operational Registers: USBSTS
#define EHCI_STS_ASS    (1 << 15)   // Async Schedule Status
#define EHCI_STS_PSS    (1 << 14)   // Periodic Schedule Status
#define EHCI_STS_REC    (1 << 13)   // Reclamation bit
#define EHCI_STS_HCH    (1 << 12)   // Host Controller Halted
#define EHCI_STS_IAA    EHCI_INT_IAA    // Interrupt on Async Advance
#define EHCI_STS_HSE    EHCI_INT_HSE    // Host System Error
#define EHCI_STS_FLR    EHCI_INT_FLR    // Frame List Rollover
#define EHCI_STS_PCD    EHCI_INT_PCD    // Port Change Detect
#define EHCI_STS_USBERR EHCI_INT_USBERR // USB Error Interrupt
#define EHCI_STS_USBINT EHCI_INT_USBINT // USB Interrupt

// Operational Registers: FRINDEX
#define EHCI_FI_FI(x)   (((x) >> 3) & 0x3ff)    // Frame Index

// Operational Registers: CONFIGFLAG
#define EHCI_CF_CF      (1 <<  0)   // Configure Flag

// Operational Registers: PORTSC
#define EHCI_PSC_WKOC_E     (1 << 22)   // Wake on Over-current Enable
#define EHCI_PSC_WKDSCNNT_E (1 << 21)   // Wake on Disconnect Enable
#define EHCI_PSC_WKCNNT_E   (1 << 20)   // Wake on Connect Enable
#define EHCI_PSC_PTC_DIS    (0 << 16)   // Port Test Control: disabled
#define EHCI_PSC_PTC_J      (1 << 16)   // Port Test Control: J-State
#define EHCI_PSC_PTC_K      (2 << 16)   // Port Test Control: K-State
#define EHCI_PSC_PTC_SE0    (3 << 16)   // Port Test Control: SE0 Nak
#define EHCI_PSC_PTC_PKT    (4 << 16)   // Port Test Control: Packet
#define EHCI_PSC_PTC_FEN    (5 << 16)   // Port Test Control: Force Enable
#define EHCI_PSC_PI_OFF     (0 << 14)   // Port Indicators: off
#define EHCI_PSC_PI_AMBER   (1 << 14)   // Port Indicators: amber
#define EHCI_PSC_PI_GREEN   (2 << 14)   // Port Indicators: green
#define EHCI_PSC_PO         (1 << 13)   // Port Owner
#define EHCI_PSC_PP         (1 << 12)   // Port Power
#define EHCI_PSC_LS_MASK    (3 << 10)   // Line Status mask
#define EHCI_PSC_LS_SE0     (0 << 10)   // Line Status: SE0
#define EHCI_PSC_LS_J       (1 << 10)   // Line Status: J-State
#define EHCI_PSC_LS_K       (2 << 10)   // Line Status: K-State
#define EHCI_PSC_RES        (1 <<  8)   // Port Reset
#define EHCI_PSC_SUS        (1 <<  7)   // Suspend
#define EHCI_PSC_FPR        (1 <<  6)   // Force Port Resume
#define EHCI_PSC_OCC        (1 <<  5)   // Over-current Change
#define EHCI_PSC_OCA        (1 <<  4)   // Over-current Active
#define EHCI_PSC_PEDC       (1 <<  3)   // Port Enable/Disable Change
#define EHCI_PSC_PED        (1 <<  2)   // Port Enabled/Disabled
#define EHCI_PSC_CSC        (1 <<  1)   // Connect Status Change
#define EHCI_PSC_CCS        (1 <<  0)   // Current Connect Status

// Capability Registers: HCSPARAMS
#define EHCI_HCS_N_CC(x)    (((x) >> 12) & 0xf) // Number of Companion
                                                // Controllers
#define EHCI_HCS_N_PCC(x)   (((x) >>  8) & 0xf) // Number of Ports per CC
#define EHCI_HCS_PRR        (1 <<  7)           // Port Routing Rules
#define EHCI_HCS_PPC        (1 <<  4)           // Port Power Control
#define EHCI_HCS_N_PORTS(x) (((x) >>  0) & 0xf) // Number of Ports

// Capability Registers: HCCPARAMS
#define EHCI_HCC_EECP(x)    (((x) >> 8) & 0xff) // EHCI Extended Capabilities
                                                // Pointer
#define EHCI_HCC_64         (1 <<  0)           // 64-bit Addressing Capability

// EHCI Extended Capability
#define EHCI_EEC_NEXT(x)    (((x) >> 8) & 0xff)
#define EHCI_EEC_ID(x)      (((x) >> 0) & 0xff)

#define EHCI_EECID_LEGSUP   0x01

// QH.ep_state[0] (Endpoint Characteristics)
#define EHCI_QHES0_RL(x)    ((uint32_t)(x) << 28) // Nak Count Reload
#define EHCI_QHES0_C        (1 << 27)   // Control Endpoint Flag
#define EHCI_QHES0_MPL(x)   ((x) << 16) // Maximum Packet Length
#define EHCI_QHES0_READ_MPL(x)  (((x) >> 16) & 0x7ff)
#define EHCI_QHES0_H        (1 << 15)   // Head of Reclamation List flag
#define EHCI_QHES0_DTC      (1 << 14)   // Data Toggle Control
#define EHCI_QHES0_EPS_MASK (3 << 12)   // Endpoint Speed mask
#define EHCI_QHES0_EPS_FS   (0 << 12)   // Endpoint Speed: Full Speed
#define EHCI_QHES0_EPS_LS   (1 << 12)   // Endpoint Speed: Low Speed
#define EHCI_QHES0_EPS_HS   (2 << 12)   // Endpoint Speed: High Speed
#define EHCI_QHES0_EP(x)    ((x) <<  8) // Endpoint Number
#define EHCI_QHES0_I        (1 <<  7)   // Inactive on Next Transaction
#define EHCI_QHES0_DEV(x)   ((x) <<  0) // Device Address

// QH.ep_state[1] (Endpoint Capabilities)
#define EHCI_QHES1_MULT(x)  ((uint32_t)(x) << 30) // High-Bandwidth Pipe
                                                  // Multiplier
#define EHCI_QHES1_PORT(x)  ((x) << 23) // Port Number
#define EHCI_QHES1_HUB(x)   ((x) << 16) // Hub Address
#define EHCI_QHES1_SCM(x)   ((x) <<  8) // Split Completion Mask
#define EHCI_QHES1_ISM(x)   ((x) <<  0) // Interrupt Schedule Mask

// qTD.status (Token)
#define EHCI_QTDS_DT(x)     ((uint32_t)(x) << 31) // Data Toggle
#define EHCI_QTDS_XFLEN(x)  ((x) << 16) // Total Bytes to Transfer
#define EHCI_QTDS_IOC       (1 << 15)   // Interrupt on Complete
#define EHCI_QTDS_CP(x)     ((x) << 12) // Current Page
#define EHCI_QTDS_CERR(x)   ((x) << 10) // Error Counter
#define EHCI_QTDS_PID_OUT   (0 <<  8)   // PID: OUT
#define EHCI_QTDS_PID_IN    (1 <<  8)   // PID: IN
#define EHCI_QTDS_PID_SETUP (2 <<  8)   // PID: SETUP
#define EHCI_QTDS_ACTIVE    (1 <<  7)   // Status: Active
#define EHCI_QTDS_HALTED    (1 <<  6)   // Status: Halted
#define EHCI_QTDS_BUFERR    (1 <<  5)   // Status: Data Buffer Error
#define EHCI_QTDS_BABBLE    (1 <<  4)   // Status: Babble Detected
#define EHCI_QTDS_XACTERR   (1 <<  3)   // Status: Transaction Error
#define EHCI_QTDS_MuF       (1 <<  2)   // Status: Missed Microframe
#define EHCI_QTDS_SPLITXS   (1 <<  1)   // Status: Split Transaction State
#define EHCI_QTDS_P_ERR     (1 <<  0)   // Status: Ping State / Error

// High Speed Error Mask
#define EHCI_QTDS_HS_ERR_MASK \
    (EHCI_QTDS_HALTED | EHCI_QTDS_BUFERR | EHCI_QTDS_BABBLE | \
     EHCI_QTDS_XACTERR | EHCI_QTDS_MuF)

// Low/Full Speed Error Mask
#define EHCI_QTDS_LFS_ERR_MASK \
    (EHCI_QTDS_HS_ERR_MASK | EHCI_QTDS_P_ERR)


struct cdi_device *ehci_init_device(struct cdi_bus_data *bus_data);

void ehci_rh_port_down(struct cdi_usb_hub *hub, int index);
void ehci_rh_port_up(struct cdi_usb_hub *hub, int index);
cdi_usb_port_status_t ehci_rh_port_status(struct cdi_usb_hub *hub, int index);

cdi_usb_hc_transaction_t
    ehci_create_transaction(struct cdi_usb_hc *hc,
                            const struct cdi_usb_hc_ep_info *target);
void ehci_enqueue(struct cdi_usb_hc *hc,
                  const struct cdi_usb_hc_transmission *trans);
void ehci_wait_transaction(struct cdi_usb_hc *hc, cdi_usb_hc_transaction_t ta);
void ehci_destroy_transaction(struct cdi_usb_hc *hc,
                              cdi_usb_hc_transaction_t ta);

#endif
