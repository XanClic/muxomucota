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

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cdi.h>
#include <cdi/misc.h>
#include <cdi/scsi.h>
#include <cdi/usb.h>

#include "usb-storage.h"


#define CBW_IN  0x80
#define CBW_OUT 0x00

#define CBW_SIGNATURE 0x43425355 // 'USBC' => USB command
#define CSW_SIGNATURE 0x53425355 // 'USBS' => USB status

struct msd_bo_cbw {
    uint32_t signature;
    uint32_t tag;
    uint32_t data_length;
    uint8_t flags;
    uint8_t lun;
    uint8_t cb_length;
    uint8_t cb[16];
} __attribute__((packed));

struct msd_bo_csw {
    uint32_t signature;
    uint32_t tag;
    uint32_t data_residue;
    uint8_t status;
} __attribute__((packed));


typedef enum usb_mass_storage_type {
    USBMS_BULK_ONLY,
} usb_mass_storage_type_t;


typedef struct usb_mass_storage_device {
    struct cdi_scsi_device dev;
    struct cdi_usb_device *usb;

    usb_mass_storage_type_t type;
    int luns;

    // FIXME: put into own class
    int bulk_in, bulk_out;
} usb_mass_storage_device_t;


static int bulk_only_reset(usb_mass_storage_device_t *dev)
{
    cdi_usb_transmission_result_t res;

    res = cdi_usb_control_transfer(dev->usb, 0, &(struct cdi_usb_setup_packet){
            .bm_request_type    = CDI_USB_CREQ_CLASS | CDI_USB_CREQ_INTERFACE
                                | CDI_USB_CREQ_OUT,
            .b_request          = USBMS_REQ_BOMSR,
            .w_index            = dev->usb->interface
        }, NULL);
    if (res != CDI_USB_OK) {
        return -EIO;
    }

    if (dev->luns > 0) {
        return 0;
    }

    uint8_t lun_count;
    res = cdi_usb_control_transfer(dev->usb, 0, &(struct cdi_usb_setup_packet) {
            .bm_request_type    = CDI_USB_CREQ_CLASS | CDI_USB_CREQ_INTERFACE
                                | CDI_USB_CREQ_IN,
            .b_request          = USBMS_REQ_GML,
            .w_index            = dev->usb->interface,
            .w_length           = 1
        }, &lun_count);
    if (res == CDI_USB_OK) {
        dev->luns = lun_count + 1;
    } else {
        dev->luns = 1;
    }

    return 0;
}


struct cdi_device *init_usb_device(struct cdi_bus_data *bus_data)
{
    static int dev_counter = 0;
    usb_mass_storage_device_t *dev = calloc(1, sizeof(*dev));
    dev->usb = CDI_UPCAST(bus_data, struct cdi_usb_device, bus_data);

    if ((dev->usb->subclass_id == USBMS_SUBCLS_SCSILEG ||
         dev->usb->subclass_id == USBMS_SUBCLS_SCSI) &&
        dev->usb->protocol_id == USBMS_PROTO_BBB)
    {
        dev->type = USBMS_BULK_ONLY;
    } else {
        free(dev);
        return NULL;
    }

    if (dev->type == USBMS_BULK_ONLY) {
        if (bulk_only_reset(dev) < 0) {
            free(dev);
            return NULL;
        }

        struct cdi_usb_endpoint_descriptor ep_desc;
        for (int ep = 1; ep < dev->usb->endpoint_count; ep++) {
            cdi_usb_get_endpoint_descriptor(dev->usb, ep, &ep_desc);

            if ((ep_desc.bm_attributes & CDI_USB_EPDSC_ATTR_XFER_TYPE_MASK)
                == CDI_USB_EPDSC_ATTR_BULK)
            {
                bool is_in;
                is_in = ep_desc.b_endpoint_address & CDI_USB_EPDSC_EPADR_DIR;
                if (!dev->bulk_in && is_in) {
                    dev->bulk_in = ep;
                } else if (dev->bulk_in && !is_in) {
                    dev->bulk_out = ep;
                }
            }
        }

        if (!dev->bulk_in || !dev->bulk_out) {
            free(dev);
            return NULL;
        }
    }

    dev->dev.dev.name = malloc(5);
    sprintf((char *)dev->dev.dev.name, "msd%c", 'a' + dev_counter++);

    dev->dev.type = CDI_STORAGE;
    dev->dev.lun_count = dev->luns;

    return &dev->dev.dev;
}


int usb_storage_request(struct cdi_scsi_device *device,
                        struct cdi_scsi_packet *packet)
{
    cdi_usb_transmission_result_t res;
    usb_mass_storage_device_t *dev = CDI_UPCAST(device,
                                                usb_mass_storage_device_t, dev);

    struct msd_bo_cbw cbw = {
        .signature      = CBW_SIGNATURE,
        .tag            = 42,
        .data_length    = packet->bufsize,
        .flags          = packet->direction == CDI_SCSI_READ ? CBW_IN : CBW_OUT,
        .lun            = packet->lun,
        .cb_length      = packet->cmdsize
    };
    memcpy(cbw.cb, packet->command, sizeof(cbw.cb));

    res = cdi_usb_bulk_transfer(dev->usb, dev->bulk_out, &cbw, sizeof(cbw));
    if (res != CDI_USB_OK) {
        bulk_only_reset(dev);
        return -1;
    }

    if (packet->direction == CDI_SCSI_READ) {
        res = cdi_usb_bulk_transfer(dev->usb, dev->bulk_in,
                                    packet->buffer, packet->bufsize);
    } else if (packet->direction == CDI_SCSI_WRITE) {
        res = cdi_usb_bulk_transfer(dev->usb, dev->bulk_out,
                                    packet->buffer, packet->bufsize);
    }

    if (res != CDI_USB_OK) {
        bulk_only_reset(dev);
        return -1;
    }

    struct msd_bo_csw csw;
    res = cdi_usb_bulk_transfer(dev->usb, dev->bulk_in, &csw, sizeof(csw));

    if (res != CDI_USB_OK || csw.signature != CSW_SIGNATURE || csw.tag != 42 ||
        csw.data_residue || csw.status)
    {
        bulk_only_reset(dev);
        return -1;
    }

    return 0;
}
