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

#ifndef USB_STORAGE_H
#define USB_STORAGE_H

#include <cdi.h>
#include <cdi/scsi.h>


// Mass storage subclass codes
#define USBMS_SUBCLS_SCSILEG    0x00 // SCSI legacy ("command set not reported")
#define USBMS_SUBCLS_RBC        0x01 // RBC
#define USBMS_SUBCLS_MMC5       0x02 // MMC-5 (ATAPI)
#define USBMS_SUBCLS_UFI        0x04 // UFI for Floppy Disk Drives
#define USBMS_SUBCLS_SCSI       0x06 // SCSI transparent command set
#define USBMS_SUBCLS_LSDFS      0x07 // LSD FS for negotiating SCSI access
#define USBMS_SUBCLS_IEEE1667   0x08 // IEEE 1667
#define USBMS_SUBCLS_VENDOR     0xff // Vendor-specific

// Mass storage protocol codes
#define USBMS_PROTO_CBI_CCI     0x00 // Control/Bulk/Interrupt with
                                     // Command Completion Interrupt
#define USBMS_PROTO_CBI         0x01 // CBI without CCI
#define USBMS_PROTO_BBB         0x50 // Bulk-only
#define USBMS_PROTO_UAS         0x62 // UAS
#define USBMS_PROTO_VENDOR      0xff // Vendor-specific

// Mass storage request codes
#define USBMS_REQ_ADSC  0x00 // Accept Device-Specific Command (CBI)
#define USBMS_REQ_GETRQ 0xfc // Get Requests (LSD FS)
#define USBMS_REQ_PUTRQ 0xfd // Put Requests (LSD FS)
#define USBMS_REQ_GML   0xfe // Get Max LUN (Bulk-only)
#define USBMS_REQ_BOMSR 0xff // Bulk-Only Mass Storage Reset (Bulk-only)


struct cdi_device *init_usb_device(struct cdi_bus_data *bus_data);
int usb_storage_request(struct cdi_scsi_device *device,
                        struct cdi_scsi_packet *packet);

#endif
