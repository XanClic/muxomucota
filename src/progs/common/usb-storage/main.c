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

#include <cdi.h>
#include <cdi/scsi.h>
#include <cdi/usb.h>

#include "usb-storage.h"


#define DRIVER_NAME "usb-storage"


static struct cdi_scsi_driver drv;
static struct cdi_usb_bus_device_pattern bus_pattern;

static int drv_init(void)
{
    cdi_scsi_driver_init(&drv);
    cdi_handle_bus_device(&drv.drv, &bus_pattern.pattern);
    return 0;
}

static int drv_destroy(void)
{
    cdi_driver_destroy(&drv.drv);
    return 0;
}


static struct cdi_scsi_driver drv = {
    .drv = {
        .name               = DRIVER_NAME,
        .type               = CDI_SCSI,
        .bus                = CDI_USB,
        .init               = drv_init,
        .destroy            = drv_destroy,
        .init_device        = init_usb_device,
    },

    .request    = usb_storage_request,
};

static struct cdi_usb_bus_device_pattern bus_pattern = {
    .pattern = {
        .bus_type = CDI_USB,
    },

    .vendor_id  = -1,
    .product_id = -1,

    .class_id       = CDI_USB_IFCCLS_STORAGE,
    .subclass_id    = -1,
    .protocol_id    = -1,
};

CDI_DRIVER(DRIVER_NAME, drv)
